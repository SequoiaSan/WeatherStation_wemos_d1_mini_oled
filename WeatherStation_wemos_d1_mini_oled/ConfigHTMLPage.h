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
        <style>
            .body {
                background-color: white;

            }

            .centred-container {
                width: 350px;
                background-color: rgb(186, 225, 214);
                margin: 0px auto;
                border-radius: 10px;
            }

            .main-container {
                padding-top: 25px;
                padding-bottom: 10px;
                padding-left: 20px;
                padding-right: 20px;
            }

            .head-text {
                font-size: 30px;
                text-align: center;
                font-weight: bolder;
            }

            .section {
                border-radius: 10px;

                margin-top: 10px;
                margin-bottom: 10px;
                
                padding-left: 10px;
                padding-right: 10px;
            }

            .parametr-section {
                padding: 10px;
            }

            .parametr-name {
                font-size: medium;
                text-align: left;
            }

            .parametr-input {
                background-color: transparent;
                border: 0px;
                height: 25px;
                width: 100%%;
                color: #000000;
                outline: none;
                border-bottom: 1px solid rgb(107, 107, 107);
            }

            .map {
                width: 100%%;
                height: 170px;
                margin: 0px;
            }

            .dndGrid {
                display: grid;
                grid-template-columns: repeat(2, 1fr);
                grid-auto-rows: 15px;
                margin-bottom: 10px;
            }

            .buttonGrid {
                margin: 10px;
                padding: 10px;
                padding-top: 40px;
                display: grid;
                grid-template-columns: repeat(2, 1fr);
                grid-auto-rows: 50px;
            }

            .button {
                background-color: #6FC7B2; /* Green */
                border: none;
                color: white;
                display: inline-block;
                transition-duration: 0.2s;
                line-height: 18px;
                justify-content: center;
                cursor: pointer;
            }

            .button:hover {
                background-color: #3ce2bb;
            }

            .buttonBig {
                padding: 15px 32px;
                font-size: 16px;
            }

            .buttonSmall {
                width: 120px;
                padding-top: 10px;
                padding-bottom: 10px;
                font-size: 12px;
            }

            .buttonDanger {
                background-color: #F392BD;
            }

            .buttonDanger:hover {
                background-color: #eb5498;
            }

            .help-reset {
                display: none;

                line-height: 40px;
                text-align: center;
                font-size: 30;
                font-weight: bolder;

                position: absolute;
                width: 400px;
                height: 300px;
                z-index: 15;
                top: 50%%;
                left: 50%%;
                margin: -150px 0 0 -2000px;
            }

            /* iOS style switch */
            .form-switch {
                display: inline-block;
                cursor: pointer;
                -webkit-tap-highlight-color: transparent;
            }

            .form-switch i {
                position: relative;
                display: inline-block;
                margin-right: .5rem;
                width: 46px;
                height: 26px;
                background-color: #e6e6e6;
                border-radius: 23px;
                vertical-align: text-bottom;
                transition: all 0.3s linear;
            }

            .form-switch i::before {
                content: "";
                position: absolute;
                left: 0px;
                width: 42px;
                height: 22px;
                background-color: #fff;
                border-radius: 11px;
                transform: translate3d(2px, 2px, 0) scale3d(1, 1, 1);
                transition: all 0.25s linear;
            }

            .form-switch i::after {
                content: "";
                position: absolute;
                left: 0px;
                width: 22px;
                height: 22px;
                background-color: #fff;
                border-radius: 11px;
                box-shadow: 0 2px 2px rgba(0, 0, 0, 0.24);
                transform: translate3d(2px, 2px, 0);
                transition: all 0.2s ease-in-out;
            }

            .form-switch:active i::after {
                width: 28px;
                transform: translate3d(2px, 2px, 0);
            }

            .form-switch:active input:checked + i::after { transform: translate3d(16px, 2px, 0); }

            .form-switch input { display: none; }

            .form-switch input:checked + i { background-color: #4BD763; }

            .form-switch input:checked + i::before { transform: translate3d(18px, 2px, 0) scale3d(0, 0, 0); }

            .form-switch input:checked + i::after { transform: translate3d(22px, 2px, 0); }

            /* Timer */
            .timer {
                position: absolute;
                width: 300px;
                height: 300px;
                z-index: 15;
                top: 50%%;
                left: 50%%;
                margin: -150px 0 0 -150px;
            }

            .base-timer {
                width: 300px;
                height: 300px;
            }

            .base-timer__svg {
                transform: scaleX(-1);
            }

            .base-timer__circle {
                fill: none;
                stroke: none;
            }

            .base-timer__path-elapsed {
                stroke-width: 7px;
                stroke: grey;
            }

            .base-timer__path-remaining {
                stroke-width: 7px;
                stroke-linecap: round;
                transform: rotate(90deg);
                transform-origin: center;
                transition: 1s linear all;
                fill-rule: nonzero;
                stroke: currentColor;
            }

            .base-timer__path-remaining.green {
                color: rgb(65, 184, 131);
            }

            .base-timer__path-remaining.orange {
                color: orange;
            }

            .base-timer__path-remaining.red {
                color: red;
            }

            .base-timer__label {
                position: absolute;
                width: 300px;
                height: 300px;
                top: 0px;
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 48px;
            }
        </style>

        <script>
            function sendRequest(request)
            {
                const xhr = new XMLHttpRequest();
                xhr.open('GET', request, true);
               
                if(request === '/resetdevice') {
                    startTimer(20, 'help-reset');
                }
                else {
                    startTimer(20);
                }
                xhr.send(null);
            }

            function hideSection(calle, elementId) {
                var x = document.getElementById(elementId);
                if (calle.checked === true) {
                    x.style.display = "block";
                } else {
                    x.style.display = "none";
                }
            }

            document.addEventListener('DOMContentLoaded', function() {
                hideSection(document.getElementById('screensaver_checkbox'), 'screensaver_input');
                hideSection(document.getElementById('dnd_checkbox'), 'dnd_input');
                hideSection(document.getElementById('celsius_checkbox'), 'celsius_sign');
            }, false);
        </script>

    <script data-name="BMC-Widget" data-cfasync="false" src="https://cdnjs.buymeacoffee.com/1.0.0/widget.prod.min.js" data-id="sequoiasan" data-description="Support me on Buy me a coffee!" data-message="" data-color="#ff813f" data-position="Right" data-x_margin="18" data-y_margin="18"></script>
    
    </head>
    <body>
        <div id="app" class="timer"></div>
        <div id="help-reset" class="help-reset">
            Device internal memory was reset.
            <br>
            Please, connect to WPConfig access point to connect Weather Display to your WiFi.
        </div>
        <div class="centred-container" id="main">
            <div class="main-container">
                <form action="/saveconfig" target="hidden-form">
                    <div class="head-text">Weather Display</div>
                    <div class="head-text" style="font-size:27px; font-weight: lighter;">Configuration Page</div>
                    <div class="section" style="background-color: #FDFAD6; margin-top: 20px;">
                        <div class="parametr-section">
                            <div class="parametr-name">Weather Display WiFi Client</div>
                            <input type="text" name="wifiName" class="parametr-input" value="%wifiName%"/>
                        </div>
                        <div class="parametr-section">
                            <div class="parametr-name">Coordinates</div>
                            <input type="text" name="coordinates" class="parametr-input" value="%coordinates%"/>
                        </div>
                        <div class="parametr-section">
                            <iframe class="map" frameborder="0" scrolling="no" id="iframemap" src="https://maps.google.com/maps?q=%lat%,%lon%&hl=en&z=14&output=embed"></iframe>
                        </div>
                        <div class="parametr-section">
                            <div class="parametr-name">API Key</div>
                            <input type="text" name="apiKey" class="parametr-input" value="%apiKey%"/>
                        </div>
                    </div>

                    <div class="section" style="background-color: #FDF2E8;">
                        <div class="parametr-section">
                            <label class="form-switch" style="padding-bottom: 10px;"><input type="checkbox" name="screenSaver" onclick="hideSection(this, 'screensaver_input')" id="screensaver_checkbox" %screenSaver%><i></i><a style="position:relative; top:-4px">Screen Saver</a></label>
                            <div id="screensaver_input">
                                <div class="parametr-name">Display off interval (minutes)</div>
                                <input type="number" name="screenSaverTime" class="parametr-input" value="%screenSaverTime%"/>
                            </div>
                        </div>
                    </div>

                    <div class="section" style="background-color: #F7E9F2;">
                        <div class="parametr-section">
                            <label class="form-switch" style="padding-bottom: 10px;"><input type="checkbox" name="DNDMode" onclick="hideSection(this, 'dnd_input')" id="dnd_checkbox" %DNDMode%><i></i><a style="position:relative; top:-4px">DND Mode</a></label>
                            <div id="dnd_input">
                                <div class="dndGrid"> 
                                    <div class="parametr-name">DND From (hour)</div>
                                    <div class="parametr-name">To</div>
                                    <input type="number" name="DNDFrom" class="parametr-input" style="width: 100px;" value="%DNDFrom%"/>
                                    <input type="number" name="DNDTo" class="parametr-input" style="width: 100px;" value="%DNDTo%"/>
                                </div>
                            </div>
                        </div>
                    </div>

                    <div class="section" style="background-color: #E1E1F1;">
                        <div class="parametr-section">
                            <label class="form-switch" style="padding-bottom: 10px;"><input type="checkbox" name="celsius" onclick="hideSection(this, 'celsius_sign')" id="celsius_checkbox" %celsius%><i></i><a style="position:relative; top:-4px">Celsius</a></label>
                            <br>
                            <label class="form-switch" style="padding-bottom: 10px;" id="celsius_sign"><input type="checkbox" name="celsiusSign" %celsiusSign%><i></i><a style="position:relative; top:-4px">Celsius sign display</a></label>
                        </div>
                    </div>

                    <input class="button buttonBig" style="margin: auto; display: flex; margin-top: 30px;" type="submit" value="Save Settings">
                </form>
                <div class="buttonGrid">
                    <div>
                        <button class="button buttonSmall" style="margin: auto; display: flex;" onclick="sendRequest('/restartdevice')">Restart Device</button>
                    </div>
                    <div>
                        <a style="text-decoration: none;" href="/update"><button class="button buttonSmall" style="margin: auto; display: flex;" onclick="">Update Page</button></a>
                    </div>
                    <div>
                        <button class="button buttonSmall buttonDanger" style="margin: auto; display: flex;" onclick="sendRequest('/resetdevice')">Reset Memory</button>
                    </div>
                    <div>
                        %telemetry%
                    </div>
                </div>
            </div>
        </div>
    </body>

    <script>
        // Credit: Mateusz Rybczonec

        const FULL_DASH_ARRAY = 283;
        const WARNING_THRESHOLD = 10;
        const ALERT_THRESHOLD = 5;

        const COLOR_CODES = {
        info: {
            color: "green"
        },
        warning: {
            color: "green",
            threshold: WARNING_THRESHOLD
        },
        alert: {
            color: "green",
            threshold: ALERT_THRESHOLD
        }
        };

        const TIME_LIMIT = 20;
        let TimeLimit = TIME_LIMIT;
        let timePassed = 0;
        let timeLeft = TimeLimit;
        let timerInterval = null;
        let remainingPathColor = COLOR_CODES.info.color;

        function onTimesUp(showElementId) {
            if(showElementId) {
                document.getElementById(showElementId).style.display = 'block';
                document.getElementById("app").style.display = 'none';
            }
            else {
                document.location.reload(true);
            }
            //clearInterval(timerInterval);
        }

        function startTimer(timeLimitArg, showElementId) {
            timeLeft = TimeLimit = timeLimitArg;

            document.getElementById("main").style.display = 'none';

            document.getElementById("app").innerHTML = `
                <div class="base-timer">
                <svg class="base-timer__svg" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                    <g class="base-timer__circle">
                    <circle class="base-timer__path-elapsed" cx="50" cy="50" r="45"></circle>
                    <path
                        id="base-timer-path-remaining"
                        stroke-dasharray="283"
                        class="base-timer__path-remaining ${remainingPathColor}"
                        d="
                        M 50, 50
                        m -45, 0
                        a 45,45 0 1,0 90,0
                        a 45,45 0 1,0 -90,0
                        "
                    ></path>
                    </g>
                </svg>
                <span id="base-timer-label" class="base-timer__label">${formatTime(
                    timeLeft
                )}</span>
                </div>
                `;

            timerInterval = setInterval(() => {
                timePassed = timePassed += 1;
                timeLeft = TimeLimit - timePassed;
                document.getElementById("base-timer-label").innerHTML = formatTime(
                timeLeft
                );
                setCircleDasharray();
                setRemainingPathColor(timeLeft);

                if (timeLeft === 0) {
                    onTimesUp(showElementId);
                }
            }, 1000);
        }

        function formatTime(time) {
            const minutes = Math.floor(time / 60);
            let seconds = time % 60;

            if (seconds < 10) {
                seconds = `0${seconds}`;
            }

            return `${minutes}:${seconds}`;
        }

        function setRemainingPathColor(timeLeft) {
            const { alert, warning, info } = COLOR_CODES;
            if (timeLeft <= alert.threshold) {
                document
                .getElementById("base-timer-path-remaining")
                .classList.remove(warning.color);
                document
                .getElementById("base-timer-path-remaining")
                .classList.add(alert.color);
            } else if (timeLeft <= warning.threshold) {
                document
                .getElementById("base-timer-path-remaining")
                .classList.remove(info.color);
                document
                .getElementById("base-timer-path-remaining")
                .classList.add(warning.color);
            }
        }

        function calculateTimeFraction() {
            const rawTimeFraction = timeLeft / TimeLimit;
            return rawTimeFraction - (1 / TimeLimit) * (1 - rawTimeFraction);
        }

        function setCircleDasharray() {
            const circleDasharray = `${(
                calculateTimeFraction() * FULL_DASH_ARRAY
            ).toFixed(0)} 283`;
            document
                .getElementById("base-timer-path-remaining")
                .setAttribute("stroke-dasharray", circleDasharray);
        }
    </script>
</html>
)rawliteral";
