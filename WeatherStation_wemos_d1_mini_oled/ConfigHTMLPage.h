const char* PARAM_WIFINAME = "wifiName";
const char* PARAM_LAT = "lat";
const char* PARAM_LON = "lon";
const char* PARAM_SCREENSAVER = "screenSaver";
const char* PARAM_SCREENSAVERTIME = "screenSaverTime";
const char* PARAM_DNDMODE = "DNDMode";
const char* PARAM_DNDFROM = "DNDFrom";
const char* PARAM_DNDTO = "DNDTo";
const char* PARAM_CELSIUS = "celsius";
const char* PARAM_CELSIUSSIGN = "celsiusSign";
const char* PARAM_APIKEY = "apiKey";
const char* PARAM_TELEMETRY = "telemetry";
const char* PARAM_COORDINATES = "coordinates";

// ADD DISPLAY ROTATION
const char config_html_page[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
    <head>
        <title>Weather Display Configuration Page</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <script>
            var satellite;

            function submitMessage() 
            {
                //alert("All data ");
                //document.location.reload(false);
                //setTimeout(function(){ document.location.reload(false); }, 500);   
            }

            function getLocation() {
                if (navigator.geolocation) {
                    navigator.geolocation.getCurrentPosition(setPosition,function(){alert("Geolocation is not supported by this browser.");}, {maximumAge:60000, timeout:5000, enableHighAccuracy:true});
                } else {
                    alert("Geolocation is not supported by this browser.");
                }
            }

            function setPosition(position) 
            {
                let latitude = position.coords.latitude.toFixed(4);
                let longitude = position.coords.longitude.toFixed(4);
                let url = "https://maps.google.com/maps?q=" + latitude + "," + longitude + "&hl=en&z=14&output=embed";

                document.getElementById("lat").value = latitude;
                document.getElementById("lon").value = longitude;
                document.getElementById("iframemap").src = url;
                document.getElementById("maplink").href = url;
            }

            //function reqListener () 
            //{
            //  console.log(this.responseText);
            //}

            function RestartDevice()
            {
              var oReq = new XMLHttpRequest();
              //oReq.addEventListener("restartdevice", reqListener);
              oReq.open("GET", "/restartdevice");
              oReq.send();
            }

            function ResetInternalMemmory()
            {
              var oReq = new XMLHttpRequest();
              //oReq.addEventListener("resetdevice", reqListener);
              oReq.open("GET", "/resetdevice");
              oReq.send();
            }
        </script>
    </head>
    <body>
        <!-- LAST ERROR DISPLAY!!!! -->
        <form action="/saveconfig" target="hidden-form">
            Weather Display WiFi Client <input type="text" name="wifiName" value="%wifiName%"><br><br>
            Coordinates <input type="text" id="coordinates" name="coordinates" value="%coordinates%"><!--<input id="find-loc" type="button" class="submitOn" value="Set My Coordinates" onclick="getLocation();">--><br>
            <iframe width="300" height="170" frameborder="0" scrolling="no" marginheight="0" marginwidth="0" id="iframemap" src="https://maps.google.com/maps?q=%lat%,%lon%&hl=en&z=14&output=embed"></iframe>

            <br><br>

            <input type="checkbox" id="screenSaver" name="screenSaver" %screenSaver%><label for="screenSaver">Screen Saver</label><br>
            Display off time (sec) <input type="number" name="screenSaverTime" value="%screenSaverTime%"><br><br>

            <input type="checkbox" id="DNDMode" name="DNDMode" %DNDMode%><label for="DNDMode">DND Mode</label><br>
            DND from (hour) <input type="number" name="DNDFrom" value="%DNDFrom%"> to (hour) <input type="number" name="DNDTo" value="%DNDTo%"><br><br>

            <input type="checkbox" id="celsius" name="celsius" %celsius%><label for="celsius">Celsius</label><br>
            <input type="checkbox" id="celsiusSign" name="celsiusSign" %celsiusSign%><label for="celsiusSign">Celsius sign display</label><br>

            <br><br>

            API Key <input type="text" name="apiKey" value="%apiKey%"><br><br>
            <input type="submit" value="Submit" onclick="submitMessage()">
        </form>

        <br><br><br>

        <a href="/update">Update page</a><br><br>

        %telemetry%

        <br><br>
        <input type="submit" value="Restart device" onclick="RestartDevice()">
        <br><br>

        <br><br>
        <input type="submit" value="Reset internal memmory" onclick="ResetInternalMemmory()">
        <br><br>
  </body>
  </html>)rawliteral";
