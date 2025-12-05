
sub main()
    print "starting main"
    setPtpDomain("0")

    m.syncDomain = "BS1"
    m.ptp = CreateObject("roPtp")

    m.msgPort = CreateObject("roMessagePort")
    m.syncManager = CreateObject("roSyncManager", { Domain: m.syncDomain, MulticastAddress: "255.255.255.255" })
    m.syncManager.SetPort(m.msgPort)
	m.syncManager.SetEncryptionEnable(false)

    m.vPlayer = CreateObject("roVideoPlayer")
    m.vPlayer.SetPort(m.msgPort)
    m.vPlayer.SetLoopMode(True)
    m.vPlayer.SetViewMode(1) '0: ScaleToFit, 1:  LetterboxedAndCentered, 2: FillScreenAndCentered

    ' m.udpSocket = createObject("roDatagramSocket")
    ' m.udpSocket.bindToLocalPort(4242)
    ' m.udpPort = createObject("roMessagePort")
    ' m.udpSocket.setPort(m.udpPort)
    ' m.udpSocket.joinMulticastGroup("224.0.126.20")
    ' m.udpSender = createObject("roDatagramSender")

    m.samples = ParseJson(ReadAsciiFile("sd:/samples.json"))

    m.tcpServer = CreateObject("roTCPServer")
    m.tcpServerPort = CreateObject("roMessagePort")
    m.tcpServer.setPort(m.tcpServerPort)
    m.tcpServer.BindToPort(1234)
    m.tcpConnections = CreateObject("roArray",1,true)

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
        'handleSync()
        handleTCP()
        ' handleUDP()
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

        m.vPlayer.PlayFile(m.fileInfo)
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

function handleTCP()
    event = wait(0, m.tcpServerPort)
    if not event = invalid then
        print "TCP event received" + type(event)
        if type(event) = "roTCPConnectEvent" then	
            acceptTCPConnection(event)	
        elseif type(event) = "roStreamLineEvent" then
            userId = event.GetUserData()
            if type(userId) = "roInt" and m.tcpConnections[userId] <> invalid then
                print "TCP Line Event User Data: " +userId.ToStr()
                print "TCP Line Event Data: " + event.GetString()
                eventparts = event.GetString().Tokenize(" ")
                command = eventparts[0]
                if command = "HELLO" then
                    clientId = eventparts[1]
                    if m.samples[clientId] = invalid then
                        m.samples[clientId] = []
                    end if
                    print "Client connected with id: " + clientId
                    m.tcpConnections[userId].SendLine("SEQLEN "+m.vPlayer.GetDuration().ToStr())
                    for each sample in m.samples[clientId]
                        line = "SAMPLE " + sample["time"].ToStr()+" " + sample["r"].ToStr()+" " + sample["g"].ToStr()+" " + sample["b"].ToStr()
                        m.tcpConnections[userId].SendLine(line)
                    end for
                    m.tcpConnections[userId].SendLine("SEQEND")
                    ' Send response back to client
                end if		
                'm.tcpConnections[event.GetUserData()].SendLine("Msg_Received: From Connection Number:"+event.GetUserData().ToStr()+chr(13))
            end if
        elseif type(event) = "roStreamEndEvent" then			
            if type(event.GetUserData()) = "roInt" then	
                closeTCP(event.GetUserData())
            end if
            print "TCP Stream End Event"
        end if
    end if
end function


function acceptTCPConnection(connection as object) as boolean
	
	'No conditions are tested for safety as this is for testing and will always return as handled.
	index% = m.tcpConnections.count()
	conn = CreateObject("roTCPStream")
	conn.SetLineEventPort(m.tcpServerPort)
	conn.Accept(connection)
	conn.SetUserData(index%)

	'Add the connection to the array. This will keep growing until you restart the player.
	'I would normally cap this at 20 and reshuffle the index% when full
	'0=tcp0,1=invalid(closed),2=tcp2 would become 0=tcp,1=tcp2 just to manage the active connections when you have 100's
    if m.tcpConnections.count() > 20 then
        print "TCP Connections exceeded 20, resetting array"
        oldConnections = m.tcpConnections
        m.tcpConnections = CreateObject("roArray",1,true)
        index% = 0
        for each oldConn in oldConnections
            if not oldConn = invalid then
                oldConn.SetUserData(index%)
                m.tcpConnections.push(oldConn)
                index% = index% + 1
            end if
        end for
    end if
	m.tcpConnections.push(conn)
end Function

function closeTCP(index% as integer) as boolean
	'No conditions are tested for safety as this is for testing and will always return as handled.
	m.tcpConnections[index%] = invalid	
end function

function zoneMsgSend(cmd$ As String)
    print "Sending zone message: " + cmd$
	' zoneMessageCmd = CreateObject("roAssociativeArray")
	' zoneMessageCmd["EventType"] = "SEND_ZONE_MESSAGE"
	' zoneMessageCmd["EventParameter"] = cmd$
	' m.bsp.msgPort.PostMessage(zoneMessageCmd)	
	' zoneMessageCmd	=	invalid
end function

' function handleUDP()
'     msg = m.udpPort.getMessage() 
'     if not msg = invalid
'         parsed = ParseJson(msg.getString())
'         if parsed <> invalid then
'             ' print "Parsed JSON:" + FormatJson(parsed)
'             if parsed.command = "GET SAMPLES" then
'                 if parsed.client_id <> invalid then
'                     clientId = parsed.client_id
'                     if m.samples[clientId] <> invalid then
'                         response = { command: "SAMPLES", client_id: clientId, samples: m.samples[clientId] }
'                     else
'                         response = { command: "SAMPLES", client_id: clientId, samples: [] }
'                     end if
'                     print "Sending samples to client_id: " + clientId + " host: " + msg.getSourceHost() + " port: " + msg.getSourcePort().toStr()
'                     m.udpSender.SetDestination(msg.getSourceHost(), 4243)
'                     m.udpSender.Send(FormatJson(response))
'                 else
'                    print "UDP command missing client_id"
'                 end if
'                 print "Command GET SAMPLES received"
                
'             else
'                 print "Unknown UDP command:" + parsed.command
'             end if
'         else
'             print "Received non-JSON UDP message"
'         end if
'     end if
' end function


function handleSync() as void
    event = m.msgPort.getMessage()
    'print "event: " + type(event)
    if type(event) = "roSyncManagerEvent" then
        print "Sync message received"
        onSyncMessage(event)
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

