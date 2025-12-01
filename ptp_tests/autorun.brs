
sub main()
    print "starting main"
    setPtpDomain("0")

    m.syncDomain = "BS1"
    m.ptp = CreateObject("roPtp")

    msgPort = CreateObject("roMessagePort")
    m.syncManager = CreateObject("roSyncManager", { Domain: m.syncDomain })
    m.syncManager.SetPort(msgPort)
		m.syncManager.SetEncryptionEnable(false)

    m.vPlayer = CreateObject("roVideoPlayer")
    m.vPlayer.SetPort(msgPort)
    m.vPlayer.SetLoopMode(True)
    m.vPlayer.SetViewMode(1) '0: ScaleToFit, 1:  LetterboxedAndCentered, 2: FillScreenAndCentered

    ' Check for DHCP server file
    if CreateObject("roReadFile", "SD:/dhcp_server") <> invalid then
        StartDHCPServer()
    else
        StartDHCPClient()
    end if

    ' Check for "master" file and sync setup
    if CreateObject("roReadFile", "SD:/master") <> invalid then
        PlayFirstVideoAsMaster()
    else
        PlayFirstVideoAsFollower()
    end if

    while true
        event = wait(0, msgPort)
        'print "event: " + type(event)
        if type(event) = "roSyncManagerEvent" then
            print "Sync message received"
            onSyncMessage(event)
        end if
    end while
end sub

sub PlayFirstVideoAsMaster()
    files = MatchFiles("SD:/", "*.mp4")

    if files.Count() > 0 then
        m.fileInfo = { Filename: "SD:/" + files[0] }
        m.vPlayer.PreloadFile(m.fileInfo)
        m.syncManager.SetMasterMode(True)
        m.syncManager.Synchronize("sync-id-1", 1000)
        print "Playing first video as master: " + files[0]
    else
        print "No video files found on SD"
    end if
end sub

sub PlayFirstVideoAsFollower()
    files = MatchFiles("SD:/", "*.mp4")

    if files.Count() > 0 then
        m.fileInfo = { Filename: "SD:/" + files[0] }
        m.syncManager.SetMasterMode(False)
        print "Playing first video as follower: " + files[0]
        print "Waiting for sync message"
    else
        print "No video files found on SD"
    end if
end sub


function setPtpDomain(domain) as void
    regSec = CreateObject("roRegistrySection", "networking")
    if regSec.Read("ptp_domain") <> domain then
        print regSec.Read("ptp_domain")
        regSec.Write("ptp_domain", domain)
        regSec.Flush()
        RebootSystem()
    else
        print "PTP domain already set to " + domain
    end if
end function

sub onSyncMessage(msg) as void
    if m.fileInfo <> invalid then
        if msg <> invalid then
            m.fileInfo.SyncDomain = m.syncDomain
            m.fileInfo.SyncId = msg.GetId()
            m.fileInfo.SyncIsoTimestamp = msg.GetIsoTimestamp()
            ok = m.vPlayer.PlayFile(m.fileInfo)
            if ok then
                print "synced video player"
            else
                print "sync failed: video player is not ok"
            end if
        else
            print "sync failed: message invalid"
        end if
    else
        print "sync failed: fileinfo missing"
    end if

end sub

sub StartDHCPClient()
    nc = CreateObject("roNetworkConfiguration", 0)
    nc.SetDHCP()
    nc.Apply()
    print "DHCP client started"
end sub

sub StartDHCPServer()
    nc = CreateObject("roNetworkConfiguration", 0)
    nc.SetIP4Address("10.10.0.1")
    nc.SetIP4Netmask("255.255.255.0")
    nc.ConfigureDHCPServer({ ip4_start: "10.10.0.2", ip4_end: "10.10.0.255", ip4_gateway: "10.10.0.1" })
    nc.Apply()
    print "DHCP server started"
end sub

