
sub main()
    print "starting main"
    ' setPtpDomain("0")

    ' m.syncDomain = "BS1"
    ' m.ptp = CreateObject("roPtp")
    m.msgPort = CreateObject("roMessagePort")

    ' m.syncManager = CreateObject("roSyncManager", { Domain: m.syncDomain })
    ' m.syncManager.SetPort(m.msgPort)
	' m.syncManager.SetEncryptionEnable(false)

    m.vPlayer = CreateObject("roVideoPlayer")
    m.vPlayer.SetPort(m.msgPort)
    m.vPlayer.SetLoopMode(True)
    m.vPlayer.SetViewMode(1) '0: ScaleToFit, 1:  LetterboxedAndCentered, 2: FillScreenAndCentered


    m.udpSocketPort = CreateObject("roMessagePort")
    m.udpSocket = CreateObject("roDatagramSocket")
    m.udpSocket.BindToLocalPort(4242)
    m.udpSocket.SetPort(m.udpSocketPort)

    m.samples = ParseJson(ReadAsciiFile("sd:/samples.json"))
    m.devices = []

    for each device in m.samples
        m.devices.push({
            ip: invalid,
            client_id: device,
            last_seen: 0
        })
    end for

    for key = 0 to m.samples.count() - 1
        print "Loaded samples for client_id: " + (key+1).toStr() + " count: " + m.samples[key].count().ToStr()
    end for



    m.tcpServer = CreateObject("roTCPServer")
    m.tcpServerPort = CreateObject("roMessagePort")
    m.tcpServer.setPort(m.tcpServerPort)
    m.tcpServer.BindToPort(1234)
    m.tcpConnections = CreateObject("roArray", 1, true)
    m.tcpConnectionIps = CreateObject("roArray", 1, true)

    m.timerPort = CreateObject("roMessagePort")
    m.timer = CreateObject("roTimer")
    m.timer.SetPort(m.timerPort)
    m.timer.SetElapsed(1, 0)
    m.timer.Start()
    m.currentLedIndex = 0

    ' Check for DHCP server file
    if CreateObject("roReadFile", "SD:/dhcp_server") <> invalid then
        StartDHCPServer()
    else
        StartDHCPClient()
    end if

    PlayFirstVideoAsMaster()

    while true
        ' handleSync()
        handleTCP()
        ' handleVideoPlayerEvents()
        handleTimer()
        ' handleUDP()
    end while
end sub

sub PlayFirstVideoAsMaster()
    files = MatchFiles("SD:/", "*.mp4")

    if files.Count() > 0 then
        m.fileInfo = { Filename: "SD:/" + files[0] }
        m.vPlayer.PreloadFile(m.fileInfo)
        ' m.syncManager.SetMasterMode(True)
        ' m.syncManager.Synchronize("sync-id-1", 1000)
        print "Playing first video as master: " + files[0]
        ok = m.vPlayer.PlayFile(m.fileInfo)
        if ok then
            print "video player started"

            ' c = 0
            ' 'm.vPlayer.GetPlaybackPosition()
            ' print "Video duration: " + m.vPlayer.GetDuration().ToStr()
            ' print "Scheduling events for devices, total devices: " + m.devices.count().ToStr()
            ' x = 0 
            ' while x < m.vPlayer.GetDuration()
            '     d = c mod m.devices.count()
            '     print "Scheduling event at: " + x.ToStr() + " for device index: " + d.ToStr()
            '     m.vPlayer.AddEvent(x,d)
            '     c = c + 1
            '     x = x + 1000
            ' end while
    
        end if
    else
        print "No video files found on SD"
    end if
end sub

' sub handleVideoPlayerEvents()
'     event = m.msgPort.WaitMessage(1)
'     if not event = invalid then
'         print "Video Player event received: " + type(event)
'         if type(event) = "roVideoEvent" then
'          ledIndex = event.GetInt()

'          print "IP Event at LED index: " + ledIndex.ToStr()
'          else 
'             print "Unknown Video Player event type: " + type(event)
'         end if

'     end if
' end sub


sub handleTimer()
    event = m.timerPort.WaitMessage(1)
    if not event = invalid then
        print "Timer event received"
        m.timer.Start()
       
        conn = m.devices[m.currentLedIndex].ip
        currentFrame = (m.vPlayer.GetPlaybackPosition() / m.vPlayer.GetStreamInfo().VideoFramerate).toInt()
        print currentFrame
        print "Sent FRAME " + (currentFrame).toStr() + " to " + conn    
        m.udpSocket.SendTo( conn, 1234,"FRAME " + (currentFrame).toStr() )
      
        ' zoneMsgSend("PING")
   end if
    
end sub

' function setPtpDomain(domain) as void
'     regSec = CreateObject("roRegistrySection", "networking")
'     if regSec.Read("ptp_domain") <> domain then
'         print regSec.Read("ptp_domain")
'         regSec.Write("ptp_domain", domain)
'         regSec.Flush()
'         RebootSystem()
'     else
'         print "PTP domain already set to " + domain
'     end if
' end function

function handleTCP()
    event = m.tcpServerPort.WaitMessage(1)
    if not event = invalid then
        print "TCP event received" + type(event)
        if type(event) = "roTCPConnectEvent" then	
            'No conditions are tested for safety as this is for testing and will always return as handled.
            index% = m.tcpConnections.count()
            conn = CreateObject("roTCPStream")
            conn.SetLineEventPort(m.tcpServerPort)
            conn.SetUserData(index%)
            conn.Accept(event)
            m.tcpConnections[index%] = conn
            m.tcpConnectionIps[index%] = event.GetSourceAddress()
        elseif type(event) = "roStreamLineEvent" then
            userId = event.GetUserData()
            if type(userId) = "roInt" and m.tcpConnections[userId] <> invalid then
                print "TCP Line Event User Data: " +userId.ToStr()
                print "TCP Line Event Data: " + event.GetString()
                eventparts = event.GetString().Tokenize(" ")
                command = eventparts[0]
                if command = "HELLO" then
                    ledId = eventparts[1].ToInt()
                    print "Client connected with id: " + ledId.ToStr()
                    m.tcpConnections[userId].SendLine("SEQLEN "+m.vPlayer.GetDuration().ToStr())
                    if m.samples[ledId] <> invalid then
                        for each sample in m.samples[ledId]
                            line = "SAMPLE " + sample["r"].ToStr()+" " + sample["g"].ToStr()+" " + sample["b"].ToStr()
                            m.tcpConnections[userId].SendLine(line)
                        end for
                    end if
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
        else
            print "TCP event invalid" + type(event)
        end if
    end if
end function





function closeTCP(index% as integer) as boolean
	'No conditions are tested for safety as this is for testing and will always return as handled.
	m.tcpConnections[index%] = invalid	
	m.tcpConnectionIps[index%] = invalid
    print "Closed TCP connection at index: " + index%.ToStr()
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


' function handleSync() as void
'     event = m.msgPort.getMessage()
'     'print "event: " + type(event)
'     if type(event) = "roSyncManagerEvent" then
'         print "Sync message received"
'         onSyncMessage(event)
'     else
'         print "Sync event invalid: " + type(event)
'     end if

'     event = m.syncPort.getMessage()
'     'print "event: " + type(event)
'     if type(event) = "roSyncManagerEvent" then
'         print "Sync2 message received"
'         onSyncMessage(event)
'     else
'         print "Sync2 event invalid: " + type(event)
'     end if
' end function

' sub onSyncMessage(msg) as void
'     if m.fileInfo <> invalid then
'         if msg <> invalid then
'             m.fileInfo.SyncDomain = m.syncDomain
'             m.fileInfo.SyncId = msg.GetId()
'             m.fileInfo.SyncIsoTimestamp = msg.GetIsoTimestamp()
'             ok = m.vPlayer.PlayFile(m.fileInfo)
'             if ok then
'                 print "synced video player"
'             else
'                 print "sync failed: video player is not ok"
'             end if
'         else
'             print "sync failed: message invalid"
'         end if
'     else
'         print "sync failed: fileinfo missing"
'     end if

' end sub

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

