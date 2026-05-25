#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <MPU6050.h>

// Replace with your network credentials
const char* ssid = "LB_ADSL_NYZQ";
const char* password = "Xvv2p96RZYsWaSEUmd";

#define LED 23

// MPU6050 setup
MPU6050 mpu;
unsigned long lastTime = 0;
bool gameComplete = false;

// Web server setup
AsyncWebServer server(80);
AsyncEventSource events("/events");

// HTML Game
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>MPU6050 Tilt Game</title>
    <style>
        body {
            margin: 0;
            overflow: hidden;
            font-family: Arial, sans-serif;
            background: #000000;
        }
        #gameCanvas {
            display: block;
            margin: 0 auto;
        }
        #hud {
            position: absolute;
            top: 20px;
            left: 20px;
            color: white;
            font-size: 20px;
            z-index: 10;
            background: rgba(0,0,0,0.7);
            padding: 15px;
            border-radius: 10px;
        }
        #menuButton {
            position: absolute;
            top: 20px;
            right: 20px;
            z-index: 10;
            background: #333;
            color: white;
            border: 2px solid #555;
            padding: 10px 20px;
            font-size: 18px;
            cursor: pointer;
            border-radius: 8px;
        }
        #menuButton:hover {
            background: #555;
        }
        #menu {
            display: none;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(30,30,30,0.95);
            border: 2px solid #555;
            padding: 30px;
            z-index: 20;
            border-radius: 15px;
            color: white;
            text-align: center;
            min-width: 300px;
        }
        #menu h2 {
            margin-top: 0;
            color: #ff0000;
        }
        #menu label {
            display: block;
            margin: 15px 0 5px;
            font-size: 16px;
        }
        #menu input[type="range"] {
            width: 100%;
            height: 8px;
            -webkit-appearance: none;
            background: #555;
            border-radius: 5px;
            outline: none;
        }
        #menu input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 25px;
            height: 25px;
            background: #ff0000;
            border-radius: 50%;
            cursor: pointer;
        }
        #sensitivityValue {
            color: #ff0000;
            font-weight: bold;
            font-size: 20px;
        }
        #menu button {
            margin-top: 20px;
            padding: 10px 30px;
            font-size: 18px;
            background: #ff0000;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-weight: bold;
        }
        #menu button:hover {
            background: #cc0000;
        }
        #deadzoneValue {
            color: #ff6666;
            font-weight: bold;
            font-size: 20px;
        }
    </style>
</head>
<body>
    <div id="hud">
        Coins: <span id="score">0</span>/5<br>
        Tilt X: <span id="tiltX">0</span>&deg;<br>
        Tilt Y: <span id="tiltY">0</span>&deg;
    </div>
    
    <button id="menuButton" onclick="toggleMenu()">Settings</button>
    
    <div id="menu">
        <h2>Game Settings</h2>
        
        <label for="sensitivity">Sensitivity: <span id="sensitivityValue">5.0</span></label>
        <input type="range" id="sensitivity" min="1.0" max="15.0" step="0.5" value="5.0">
        
        <label for="deadzone">Deadzone: <span id="deadzoneValue">5</span>&deg;</label>
        <input type="range" id="deadzone" min="0" max="20" step="1" value="5">
        <small style="color: #888;">Higher = ignore small tilts</small>
        
        <br>
        <button onclick="toggleMenu()">Close</button>
        <button onclick="resetGame()" style="background: #ff0000; color: white;">Reset Game</button>
    </div>
    
    <canvas id="gameCanvas"></canvas>
    
    <script>
        const canvas = document.getElementById('gameCanvas');
        const ctx = canvas.getContext('2d');
        
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
        
        let sensitivity = 5.0;
        let deadzone = 5;
        
        const player = {
            x: canvas.width / 2,
            y: canvas.height / 2,
            radius: 20,
            speedX: 0,
            speedY: 0
        };
        
        let coins = [];
        const totalCoins = 5;
        let score = 0;
        let gameOver = false;
        let tiltX = 0;  // Left/Right tilt
        let tiltY = 0;  // Up/Down tilt
        
        function toggleMenu() {
            const menu = document.getElementById('menu');
            if (menu.style.display === 'block') {
                menu.style.display = 'none';
            } else {
                menu.style.display = 'block';
            }
        }
        
        function resetGame() {
            score = 0;
            gameOver = false;
            player.x = canvas.width / 2;
            player.y = canvas.height / 2;
            player.speedX = 0;
            player.speedY = 0;
            spawnCoins();
            document.getElementById('score').textContent = '0';
            document.getElementById('menu').style.display = 'none';
        }
        
        function spawnCoins() {
            coins = [];
            for (let i = 0; i < totalCoins; i++) {
                coins.push({
                    x: Math.random() * (canvas.width - 100) + 50,
                    y: Math.random() * (canvas.height - 100) + 50,
                    radius: 15,
                    collected: false
                });
            }
        }
        
        spawnCoins();
        
        document.getElementById('sensitivity').addEventListener('input', function(e) {
            sensitivity = parseFloat(e.target.value);
            document.getElementById('sensitivityValue').textContent = sensitivity;
        });
        
        document.getElementById('deadzone').addEventListener('input', function(e) {
            deadzone = parseFloat(e.target.value);
            document.getElementById('deadzoneValue').textContent = deadzone;
        });
        
        function draw() {
            ctx.fillStyle = '#000000';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
            
            ctx.strokeStyle = '#1a1a1a';
            ctx.lineWidth = 1;
            for (let i = 0; i < canvas.width; i += 50) {
                ctx.beginPath();
                ctx.moveTo(i, 0);
                ctx.lineTo(i, canvas.height);
                ctx.stroke();
            }
            for (let i = 0; i < canvas.height; i += 50) {
                ctx.beginPath();
                ctx.moveTo(0, i);
                ctx.lineTo(canvas.width, i);
                ctx.stroke();
            }
            
            coins.forEach(coin => {
                if (!coin.collected) {
                    ctx.fillStyle = '#ffffff';
                    ctx.beginPath();
                    ctx.arc(coin.x, coin.y, coin.radius, 0, Math.PI * 2);
                    ctx.fill();
                    
                    ctx.fillStyle = '#cccccc';
                    ctx.beginPath();
                    ctx.arc(coin.x, coin.y, coin.radius * 0.6, 0, Math.PI * 2);
                    ctx.fill();
                }
            });
            
            ctx.fillStyle = '#ff0000';
            ctx.beginPath();
            ctx.arc(player.x, player.y, player.radius, 0, Math.PI * 2);
            ctx.fill();
            
            if (gameOver) {
                ctx.fillStyle = 'rgba(0, 0, 0, 0.8)';
                ctx.fillRect(0, 0, canvas.width, canvas.height);
                
                ctx.fillStyle = '#ff0000';
                ctx.font = 'bold 48px Arial';
                ctx.textAlign = 'center';
                ctx.fillText('YOU WIN!', canvas.width/2, canvas.height/2 - 30);
                ctx.fillText('LED is ON!', canvas.width/2, canvas.height/2 + 30);
                
                ctx.fillStyle = '#ffffff';
                ctx.font = '24px Arial';
                ctx.fillText('Click Reset Game in Settings to play again', canvas.width/2, canvas.height/2 + 80);
            }
        }
        
        function checkCollisions() {
            coins.forEach(coin => {
                if (!coin.collected) {
                    const dx = player.x - coin.x;
                    const dy = player.y - coin.y;
                    const distance = Math.sqrt(dx * dx + dy * dy);
                    
                    if (distance < player.radius + coin.radius) {
                        coin.collected = true;
                        score++;
                        document.getElementById('score').textContent = score;
                        
                        if (score >= totalCoins) {
                            gameOver = true;
                            fetch('/complete');
                        }
                    }
                }
            });
        }
        
        function update() {
            if (gameOver) return;
            
            document.getElementById('tiltX').textContent = tiltX.toFixed(1);
            document.getElementById('tiltY').textContent = tiltY.toFixed(1);
            
            let effectiveX = 0;
            let effectiveY = 0;
            
            if (Math.abs(tiltX) > deadzone) {
                effectiveX = tiltX;
            }
            if (Math.abs(tiltY) > deadzone) {
                effectiveY = tiltY;
            }
            
            // X tilt = left/right movement, Y tilt = up/down movement
            player.speedX = effectiveX * sensitivity * 0.5;
            player.speedY = -effectiveY * sensitivity * 0.5;
            
            player.x += player.speedX;
            player.y += player.speedY;
            
            if (player.x - player.radius < 0) player.x = player.radius;
            if (player.x + player.radius > canvas.width) player.x = canvas.width - player.radius;
            if (player.y - player.radius < 0) player.y = player.radius;
            if (player.y + player.radius > canvas.height) player.y = canvas.height - player.radius;
            
            checkCollisions();
        }
        
        function gameLoop() {
            update();
            draw();
            requestAnimationFrame(gameLoop);
        }
        
        const eventSource = new EventSource('/events');
        
        eventSource.addEventListener('orientation', function(e) {
            const data = JSON.parse(e.data);
            tiltX = data.x;
            tiltY = data.y;
        });
        
        window.addEventListener('resize', function() {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
        });
        
        gameLoop();
    </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
    
    Wire.begin(21, 22);
    
    mpu.initialize();
    if (!mpu.testConnection()) {
        Serial.println("MPU6050 connection failed!");
        while(1);
    }
    Serial.println("MPU6050 ready");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nIP Address: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    server.on("/complete", HTTP_GET, [](AsyncWebServerRequest *request){
        gameComplete = true;
        digitalWrite(LED, HIGH);
        Serial.println("Game Complete! LED ON!");
        request->send(200, "text/plain", "OK");
    });

    events.onConnect([](AsyncEventSourceClient *client){
        client->send("hello", NULL, millis(), 1000);
    });
    server.addHandler(&events);
    server.begin();
}

void loop() {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    
    float accelAngleX = atan2(ay, az) * 180 / PI;  // Left/Right tilt
    float accelAngleY = atan2(ax, az) * 180 / PI;  // Up/Down tilt

    JSONVar data;
    data["x"] = accelAngleX;
    data["y"] = accelAngleY;
    data["z"] = 0;
    events.send(JSON.stringify(data).c_str(), "orientation", millis());
    
    delay(15);
}
