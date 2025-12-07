' *************************************************************
'  autorun.brs  –  loop video + progressive grid overlay
'                 (roCanvasWidget version)
' *************************************************************

' -------- CONFIG ------------------------------------------------
sub setConfig()
    m.videoPath      = "SD:/loop.mp4"      ' video to loop
    m.saveFile       = "SD:/squares.json"  ' persistence file
end sub


' -------------------------- MAIN --------------------------------
sub Main()
  
    reg = CreateObject("roRegistrySection","html")
    reg.Write("enable_web_inspector", "1")
    reg.Flush()
    setConfig()

    vm     = createobject("roVideoMode")
    
    ' --- video player --------------------------------------------
    port   = createobject("roMessagePort")
    ' player = createobject("roVideoPlayer")
    ' player.setport(port)
    ' player.setloopmode(false)
    ' player.playfile(m.videoPath)


    rect = CreateObject("roRectangle", 0, 0, vm.getResX(), vm.getResY())
    ' --- HTML widget ----------------------------------------------
    htmlWidget = CreateObject("roHTMLWidget", rect, {
        nodejs_enabled: true
        inspector_server: { port: 3000 }
        brightsign_js_objects_enabled: true
        url: "file:///sd:/index.html"
        storage_path: "SD:"
	    storage_quota: 1073741824
        'hwz_default: "on"
    })
    htmlWidget.SetPort(port)
    htmlWidget.Show()
    ' --- event loop ----------------------------------------------
    while true
        ' noop
    end while
end sub
