// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Reflection;
using System.Security.Principal;
using System.Threading;
using System.Threading.Tasks;

using Microsoft.PowerToys.Settings.UI.Library.Utilities;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace MouseWithoutBorders.UnitTests;

[TestClass]
[DoNotParallelize]
public sealed class ClipboardIpcSecurityTests
{
    [TestCleanup]
    public void CleanupServer()
    {
        IpcHelper.CreateIpcServer(true);
        using var identity = WindowsIdentity.GetCurrent();
        Assert.IsTrue(SpinWait.SpinUntil(
            () => CanCreateExclusivePipe(CurrentSessionPipeName(), identity.User!),
            TimeSpan.FromSeconds(5)));
    }

    [TestMethod]
    public async Task ClipboardServerRejectsUnexpectedHelperExecutable()
    {
        IpcHelper.CreateIpcServer(false);
        await using var client = new NamedPipeClientStream(".", CurrentSessionPipeName(), PipeDirection.InOut, PipeOptions.Asynchronous);
        await client.ConnectAsync(5000);

        Assert.AreEqual(0, await ReadUntilDisconnectedAsync(client));
    }

    [TestMethod]
    public void RepeatedClipboardStartupKeepsExistingConnectionLifetime()
    {
        IpcHelper.CreateIpcServer(false);
        var sourceField = typeof(IpcHelper).GetField("_serverTaskCancellationSource", BindingFlags.NonPublic | BindingFlags.Static);
        Assert.IsNotNull(sourceField);
        var source = (CancellationTokenSource)sourceField.GetValue(null)!;
        var lifetime = source.Token;

        // SocketStuff starts the clipboard listener again when reopening network sockets.
        IpcHelper.CreateIpcServer(false);

        Assert.IsFalse(lifetime.IsCancellationRequested, "Reopening network sockets must preserve the helper's existing RPC connection.");
        IpcHelper.CreateIpcServer(true);
        Assert.IsTrue(lifetime.IsCancellationRequested, "Application shutdown must still end the clipboard listener.");
    }

    [TestMethod]
    public async Task ClipboardClientRejectsFakeServerBeforeSendingData()
    {
        using var identity = WindowsIdentity.GetCurrent();
        await using var fakeServer = RestrictedNamedPipeServer.Create(CurrentSessionPipeName(), identity.User!);
        var connected = fakeServer.WaitForConnectionAsync();
        var client = Task.Run(IpcHelper.CreateIpcClient);

        await connected.WaitAsync(TimeSpan.FromSeconds(5));
        Assert.IsNull(await client.WaitAsync(TimeSpan.FromSeconds(5)));
        Assert.AreEqual(0, await ReadUntilDisconnectedAsync(fakeServer));
    }

    [TestMethod]
    public void ClipboardPipesForDifferentSessionsCanCoexist()
    {
        using var identity = WindowsIdentity.GetCurrent();
        using var firstSession = RestrictedNamedPipeServer.Create(MouseWithoutBordersIpc.GetClipboardPipeName(42), identity.User!);
        using var secondSession = RestrictedNamedPipeServer.Create(MouseWithoutBordersIpc.GetClipboardPipeName(43), identity.User!);

        Assert.IsFalse(firstSession.IsConnected);
        Assert.IsFalse(secondSession.IsConnected);
        Assert.ThrowsException<ArgumentOutOfRangeException>(() => MouseWithoutBordersIpc.GetClipboardPipeName(-1));
    }

    private static string CurrentSessionPipeName()
    {
        return MouseWithoutBordersIpc.GetClipboardPipeName(Process.GetCurrentProcess().SessionId);
    }

    private static bool CanCreateExclusivePipe(string pipeName, SecurityIdentifier userSid)
    {
        try
        {
            using var server = RestrictedNamedPipeServer.Create(pipeName, userSid);
            return true;
        }
        catch (System.ComponentModel.Win32Exception)
        {
            return false;
        }
    }

    private static async Task<int> ReadUntilDisconnectedAsync(Stream stream)
    {
        try
        {
            return await stream.ReadAsync(new byte[1]).AsTask().WaitAsync(TimeSpan.FromSeconds(5));
        }
        catch (IOException)
        {
            // Windows may report ERROR_PIPE_NOT_CONNECTED instead of EOF after server Disconnect().
            return 0;
        }
    }
}
