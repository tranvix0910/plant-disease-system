#include <Arduino.h>

const char PLANT_MONITOR_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <script src="https://cdn.jsdelivr.net/npm/@tailwindcss/browser@4"></script>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.7.2/css/all.min.css" integrity="sha512-Evv84Mr4kqVGRNSgIGL/F/aIDqQb7xQ2vcrdIwxfjThSH8CSR7PBEakCr51Ck+w+/U6swU2Im1vVX0SVk9ABhg==" crossorigin="anonymous" referrerpolicy="no-referrer" />
    <title>Plant Monitor Dashboard</title>
    <style>
        body { 
            font-family: 'Inter', 'Segoe UI', 'Roboto', Arial, sans-serif; 
            overflow: hidden;
            height: 100vh;
        }
        
        .dashboard-container {
            display: flex;
            height: 100vh;
            overflow: hidden;
        }
        
        .sidebar {
            position: sticky;
            top: 0;
            height: 100vh;
            overflow-y: auto;
        }
        
        .main-content {
            display: flex;
            flex-direction: column;
            height: 100vh;
            overflow: hidden;
        }
        
        .fixed-header {
            position: sticky;
            top: 0;
            z-index: 10;
        }
        
        .scrollable-content {
            flex: 1;
            overflow-y: auto;
            padding-bottom: 20px;
        }
        
        /* Scrollbar styling */
        .scrollable-content::-webkit-scrollbar {
            width: 8px;
        }
        
        .scrollable-content::-webkit-scrollbar-track {
            background: #f1f1f1;
        }
        
        .scrollable-content::-webkit-scrollbar-thumb {
            background: #888;
            border-radius: 4px;
        }
        
        .scrollable-content::-webkit-scrollbar-thumb:hover {
            background: #555;
        }
        
        /* Card styling */
        .sensor-card {
            transition: all 0.3s ease;
        }
        
        .sensor-card:hover {
            transform: translateY(-3px);
            box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05);
        }
        
        /* Bar animations */
        .bar-indicator {
            transition: width 1s ease-out, background-color 1s ease;
        }
        
        /* Button animations */
        button {
            transition: all 0.2s ease;
        }
        
        button:active {
            transform: scale(0.97);
        }
        
        /* Card animations */
        .rounded-2xl {
            transition: all 0.3s ease;
        }
        
        /* Pump animation */
        .animate-pulse {
            animation: pulse 1.5s cubic-bezier(0.4, 0, 0.6, 1) infinite;
        }
        
        @keyframes pulse {
            0%, 100% {
                opacity: 1;
            }
            50% {
                opacity: 0.5;
            }
        }
        
        /* Status changes animation */
        .status-change {
            animation: highlight 2s ease-out;
        }
        
        @keyframes highlight {
            0% {
                background-color: rgba(167, 243, 208, 0.5);
            }
            100% {
                background-color: transparent;
            }
        }
    </style>
</head>
<body class="bg-[#e6f0ea] min-h-screen text-gray-800">
<div class="dashboard-container">
    <!-- Sidebar Left -->
    <aside class="sidebar hidden md:flex flex-col w-64 bg-[#F1F1F1] px-6 py-8 justify-between shadow-md z-20">
        <div>
            <div class="flex items-center gap-3 mb-10">
                <i class="fa fa-leaf text-green-700 text-3xl"></i>
                <span class="text-2xl font-extrabold text-green-700 tracking-wide">tkplant<span class="text-green-700">.</span></span>
            </div>
            <nav class="flex flex-col gap-2">
                <a href="#" class="flex items-center gap-3 px-4 py-2 rounded-lg bg-white text-green-700 font-semibold shadow-sm"><i class="fa fa-home"></i>Dashboard</a>
                <a href="#" class="flex items-center gap-3 px-4 py-2 rounded-lg hover:bg-white text-gray-600"><i class="fa fa-file-alt"></i>Report</a>
                <a href="#" class="flex items-center gap-3 px-4 py-2 rounded-lg hover:bg-white text-gray-600"><i class="fa fa-cloud-sun"></i>Forecast</a>
                <a href="#" class="flex items-center gap-3 px-4 py-2 rounded-lg hover:bg-white text-gray-600"><i class="fa fa-tint"></i>Control</a>
                <a href="#" class="flex items-center gap-3 px-4 py-2 rounded-lg hover:bg-white text-gray-600"><i class="fa fa-cog"></i>Settings</a>
                <a href="#" class="flex items-center gap-3 px-4 py-2 rounded-lg hover:bg-white text-gray-600"><i class="fa fa-question-circle"></i>Help</a>
            </nav>
        </div>
        <div class="hidden md:block mt-10">
            <i class="fa fa-seedling text-green-400 text-6xl opacity-60 mx-auto"></i>
        </div>
    </aside>
    <!-- Main Content -->
    <main class="main-content flex-1 flex flex-col bg-[#F1F1F1]">
        <!-- Header -->
        <header class="fixed-header flex flex-col md:flex-row items-center justify-between px-6 py-6 bg-[#F1F1F1] border-b border-gray-200 shadow-sm">
            <div>
                <h2 class="text-2xl font-bold text-gray-800">Welcome, <span class="text-green-700">User!</span></h2>
                <div class="flex items-center gap-2 text-gray-400 text-sm mt-1">
                    <span id="current-date"></span>
                <span id="current-time"></span>
                    <span id="status-badge" class="ml-2 inline-flex items-center px-3 py-1 rounded-full text-xs font-semibold bg-green-100 text-green-700"><i class="fa fa-circle mr-1 text-green-500"></i>Online</span>
                </div>
            </div>
            <div class="flex items-center gap-3 mt-4 md:mt-0">
                <button class="bg-white p-2 rounded-full shadow hover:bg-gray-100"><i class="fa fa-search text-gray-500"></i></button>
                <button class="bg-white p-2 rounded-full shadow hover:bg-gray-100"><i class="fa fa-bell text-gray-500"></i></button>
            </div>
        </header>
        
        <!-- Notification Area -->
        <div id="notification-area" class="px-6 py-2 hidden">
            <div class="w-full bg-[#537D5D] text-white px-4 py-3 rounded-lg shadow-md flex items-center transition-all duration-300">
                <i class="fa fa-check-circle mr-2"></i>
                <span id="notification-message">Notification will appear here</span>
                <button class="ml-auto text-white hover:text-gray-200" onclick="hideNotification()">
                    <i class="fa fa-times"></i>
                </button>
            </div>
        </div>
        
        <!-- Scrollable Content -->
        <div class="scrollable-content pt-4">
        <!-- Sensor Card Row (Full Width) -->
        <div class="px-6 pt-6">
                <div class="w-full bg-white rounded-2xl shadow-md p-6 flex flex-col gap-4 min-w-[320px] hover:shadow-lg transition-all duration-300">
                    <h3 class="text-xl font-bold text-gray-700 flex items-center"><i class="fa fa-chart-line text-green-600 mr-2"></i>Sensor Readings</h3>
                    <div class="grid grid-cols-1 sm:grid-cols-3 gap-6 mt-2">
                    <!-- Air Humidity Card -->
                        <div class="sensor-card bg-gray-50 rounded-xl p-4 flex flex-col gap-2 shadow-sm">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <i class="fa fa-tint text-[#537D5D] text-4xl p-6"></i>
                                <div>
                                    <div class="text-2xl font-bold text-gray-800" id="humidity-value">{{HUMIDITY}}</div>
                                    <div class="text-sm text-gray-400">Air Humidity</div>
                                </div>
                            </div>
                            <i class="fa fa-arrow-up text-green-400"></i>
                        </div>
                            <div class="w-full h-4 bg-gray-200 rounded-full mt-2">
                                <div id="humidity-bar" class="bar-indicator h-4 rounded-full transition-all duration-500" style="width:{{HUMIDITY}}%;"></div>
                        </div>
                        <div class="text-sm font-bold mt-1" id="humidity-percent">{{HUMIDITY}}%</div>
                    </div>
                    <!-- Soil Moisture Card -->
                        <div class="sensor-card bg-gray-50 rounded-xl p-4 flex flex-col gap-2 shadow-sm">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <i class="fa fa-water text-[#537D5D] text-4xl p-6"></i>
                                <div>
                                    <div class="text-2xl font-bold text-gray-800" id="soil-moisture-value">{{SOIL_MOISTURE}}</div>
                                    <div class="text-sm text-gray-400">Soil Moisture</div>
                                </div>
                            </div>
                            <i class="fa fa-arrow-up text-green-400"></i>
                        </div>
                            <div class="w-full h-4 bg-gray-200 rounded-full mt-2">
                                <div id="soil-moisture-bar" class="bar-indicator h-4 rounded-full transition-all duration-500" style="width:{{SOIL_MOISTURE}}%;"></div>
                        </div>
                        <div class="text-sm font-bold mt-1" id="soil-moisture-percent">{{SOIL_MOISTURE}}%</div>
                    </div>
                    <!-- Temperature Card -->
                        <div class="sensor-card bg-gray-50 rounded-xl p-4 flex flex-col gap-2 shadow-sm">
                        <div class="flex items-center justify-between">
                            <div class="flex items-center">
                                <i class="fa fa-thermometer-half text-[#537D5D] text-4xl p-6"></i>
                                <div>
                                    <div class="text-2xl font-bold text-gray-800" id="temperature-value">{{TEMPERATURE}}</div>
                                    <div class="text-sm text-gray-400">Temperature</div>
                                </div>
                            </div>
                            <i class="fa fa-arrow-up text-green-400"></i>
                        </div>
                            <div class="w-full h-4 bg-gray-200 rounded-full mt-2">
                                <div id="temperature-bar" class="bar-indicator h-4 rounded-full transition-all duration-500" style="width:{{TEMPERATURE}}%;"></div>
                        </div>
                        <div class="text-sm font-bold mt-1" id="temperature-percent">{{TEMPERATURE}}°C</div>
                    </div>
                </div>
                    <div class="flex flex-wrap gap-2 mt-4">
                        <button id="update-data" type="button" class="text-white bg-[#537D5D] hover:bg-green-700 focus:outline-none focus:ring-4 focus:ring-green-300 font-medium rounded-full text-sm px-5 py-2.5 me-2 mb-2 transition-all duration-300"><i class="fa fa-sync-alt mr-1"></i>Update Data</button>
                        <button id="update-sheets" type="button" class="text-white bg-[#537D5D] hover:bg-green-700 focus:outline-none focus:ring-4 focus:ring-green-300 font-medium rounded-full text-sm px-5 py-2.5 me-2 mb-2 transition-all duration-300"><i class="fa fa-cloud-upload-alt mr-1"></i>Update To Google Sheets</button>
                        <button id="get-report" type="button" class="text-white bg-[#537D5D] hover:bg-green-700 focus:outline-none focus:ring-4 focus:ring-green-300 font-medium rounded-full text-sm px-5 py-2.5 me-2 mb-2 transition-all duration-300"><i class="fa fa-file-alt mr-1"></i>Daily Report</button>
                        <button id="get-analysis" type="button" class="text-white bg-[#537D5D] hover:bg-green-700 focus:outline-none focus:ring-4 focus:ring-green-300 font-medium rounded-full text-sm px-5 py-2.5 me-2 mb-2 transition-all duration-300"><i class="fa fa-search mr-1"></i>Detailed Analysis</button>
                    </div>
                </div>
            </div>
            <!-- Cards Row 2: Disease, Control, Disease Prediction, Status, Capture, Stream -->
            <div class="flex flex-col lg:flex-row gap-6 px-6 py-6">
            <!-- Watering Control Card -->
            <div class="flex-1 bg-white rounded-2xl shadow-md p-6 flex flex-col gap-4 min-w-[320px] hover:shadow-lg transition-all duration-300">
                <div class="flex items-center gap-4 mb-2">
                    <i class="fa fa-tint text-blue-500 text-3xl bg-blue-100 rounded-full border-2 border-blue-200 shadow p-4"></i>
                    <div>
                        <div class="text-lg font-bold text-gray-800">Watering Control</div>
                        <div class="text-xs text-gray-500">Smart Pump & Scheduling System</div>
                    </div>
                </div>
                <div class="bg-gray-50 rounded-xl p-4 flex flex-col items-center shadow-sm">
                    <div class="flex justify-center items-center gap-3">
                        <i id="pump-icon" class="fa fa-power-off text-gray-400 text-2xl"></i>
                        <span class="text-xl font-bold text-gray-700" id="pump-status">Off</span>
                    </div>
                    <div class="mt-2 text-sm text-gray-600 flex items-center gap-2">
                        <i class="fa fa-clock text-blue-400"></i>
                        <span id="watering-schedule">Next watering: --</span>
                    </div>
                </div>
                <div class="flex flex-wrap gap-2 mt-2">
                    <button id="start-watering" type="button" class="text-white bg-blue-500 hover:bg-blue-600 focus:outline-none focus:ring-4 focus:ring-blue-300 font-medium rounded-lg text-sm px-5 py-2.5 transition-all duration-300">
                        <i class="fa fa-play mr-1"></i>Start Watering
                    </button>
                    <input type="time" id="watering-time" class="border rounded-lg px-3 py-2 text-sm focus:outline-none focus:ring-2 focus:ring-blue-200">
                    <button id="set-watering-time" type="button" class="text-white bg-gray-700 hover:bg-gray-800 focus:outline-none focus:ring-4 focus:ring-gray-300 font-medium rounded-lg text-sm px-5 py-2.5 transition-all duration-300">
                        <i class="fa fa-clock mr-1"></i>Set Schedule
                    </button>
                </div>
            </div>
            
            <!-- Disease Prediction Card -->
            <div class="flex-1 bg-white rounded-2xl shadow-md p-6 flex flex-col gap-4 min-w-[320px] hover:shadow-lg transition-all duration-300">
                <div class="flex items-center gap-4 mb-2">
                    <i class="fa fa-microscope text-purple-500 text-3xl bg-purple-100 rounded-full border-2 border-purple-200 shadow p-4"></i>
                    <div>
                        <div class="text-lg font-bold text-gray-800">Disease Prediction</div>
                        <div class="text-xs text-gray-500">AI-Powered Plant Health Analysis</div>
                    </div>
                </div>
                <button id="predict-btn" type="button" class="text-white bg-purple-500 hover:bg-purple-600 focus:outline-none focus:ring-4 focus:ring-purple-300 font-medium rounded-lg text-sm px-5 py-2.5 transition-all duration-300">
                    <i class="fa fa-search mr-1"></i>Predict Disease
                </button>
                <div id="predict-result" class="bg-gray-50 rounded-xl p-4 mt-2">
                    <div class="flex items-center gap-2 mb-3">
                        <i class="fa fa-leaf text-green-500"></i>
                        <h3 class="font-bold text-gray-700">Plant Health Status</h3>
                    </div>
                    <div class="border-b border-gray-200 pb-2 mb-2">
                        <div class="flex justify-between items-center">
                            <span class="text-sm text-gray-600">Status:</span>
                            <span id="predicted-class" class="font-bold text-purple-700">--</span>
                        </div>
                    </div>
                    <div class="border-b border-gray-200 pb-2 mb-2">
                        <div class="flex justify-between items-center">
                            <span class="text-sm text-gray-600">Confidence:</span>
                            <span id="predicted-confidence" class="text-gray-700">--</span>
                        </div>
                    </div>
                    <div class="border-b border-gray-200 pb-2 mb-2">
                        <div class="flex justify-between items-center">
                            <span class="text-sm text-gray-600">Possible Disease:</span>
                            <span id="possible-disease" class="text-gray-700">--</span>
                        </div>
                    </div>
                    <div id="predict-meta" class="mt-2 text-xs text-gray-500"></div>
                    <div id="all-probabilities" class="mt-2 text-xs text-gray-500"></div>
                    <div id="predict-image" class="mt-3 flex justify-center"></div>
                </div>
            </div>
            
            <!-- Device Status Card -->
            <div class="flex-1 bg-white rounded-2xl shadow-md p-6 flex flex-col gap-4 min-w-[320px] hover:shadow-lg transition-all duration-300">
                <div class="flex items-center gap-4 mb-2">
                    <i class="fa fa-microchip text-green-500 text-3xl bg-green-100 rounded-full border-2 border-green-200 shadow p-4"></i>
                    <div>
                        <div class="text-lg font-bold text-gray-800">Device Status</div>
                        <div class="text-xs text-gray-500">ESP32 System Information</div>
                    </div>
                </div>
                <button id="status-btn" type="button" class="text-white bg-green-500 hover:bg-green-600 focus:outline-none focus:ring-4 focus:ring-green-300 font-medium rounded-lg text-sm px-5 py-2.5 transition-all duration-300">
                    <i class="fa fa-sync-alt mr-1"></i>Get Status
                </button>
                <div id="status-result" class="bg-gray-50 rounded-xl p-4 mt-2">
                    <div class="flex items-center gap-2 mb-3">
                        <i class="fa fa-info-circle text-green-500"></i>
                        <h3 class="font-bold text-gray-700">System Information</h3>
                    </div>
                    <div class="border-b border-gray-200 pb-2 mb-2">
                        <div class="flex justify-between items-center">
                            <span class="text-sm text-gray-600">Status:</span>
                            <span id="device-status" class="font-bold text-green-700">--</span>
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-2 mt-2">
                        <div class="flex items-center gap-2">
                            <i class="fa fa-network-wired text-gray-400 text-xs"></i>
                            <span id="device-ip" class="text-xs text-gray-700">IP: --</span>
                        </div>
                        <div class="flex items-center gap-2">
                            <i class="fa fa-wifi text-gray-400 text-xs"></i>
                            <span id="device-ssid" class="text-xs text-gray-700">SSID: --</span>
                        </div>
                        <div class="flex items-center gap-2">
                            <i class="fa fa-clock text-gray-400 text-xs"></i>
                            <span id="device-time" class="text-xs text-gray-700">Time: --</span>
                        </div>
                        <div class="flex items-center gap-2">
                            <i class="fa fa-calendar text-gray-400 text-xs"></i>
                            <span id="device-date" class="text-xs text-gray-700">Date: --</span>
                        </div>
                        <div class="flex items-center gap-2">
                            <i class="fa fa-hourglass-half text-gray-400 text-xs"></i>
                            <span id="device-uptime" class="text-xs text-gray-700">Uptime: --</span>
                        </div>
                        <div class="flex items-center gap-2">
                            <i class="fa fa-memory text-gray-400 text-xs"></i>
                            <span id="device-freeheap" class="text-xs text-gray-700">Free Heap: --</span>
                        </div>
                        <div class="flex items-center gap-2 col-span-2">
                            <i class="fa fa-link text-gray-400 text-xs"></i>
                            <span id="device-apiurl" class="text-xs text-gray-700">API URL: --</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <!-- Capture and Stream Cards Row -->
        <div class="flex flex-col lg:flex-row gap-6 px-6 pb-8">
            <!-- Capture Image Card -->
            <div class="flex-1 bg-white rounded-2xl shadow-md p-6 flex flex-col gap-4 min-w-[320px] hover:shadow-lg transition-all duration-300">
                <div class="flex items-center gap-4 mb-2">
                    <i class="fa fa-camera text-blue-500 text-3xl bg-blue-100 rounded-full border-2 border-blue-200 shadow p-4"></i>
                    <div>
                        <div class="text-lg font-bold text-gray-800">Capture Image</div>
                        <div class="text-xs text-gray-500">Latest Plant Photo</div>
                    </div>
                </div>
                <button id="capture-btn" type="button" class="text-white bg-blue-500 hover:bg-blue-600 focus:outline-none focus:ring-4 focus:ring-blue-300 font-medium rounded-lg text-sm px-5 py-2.5 transition-all duration-300">
                    <i class="fa fa-camera mr-1"></i>Capture Photo
                </button>
                <div class="flex justify-center mt-2">
                    <img id="capture-image" src="" alt="Captured" class="rounded-xl h-48 object-contain border border-gray-200 bg-gray-50">
                </div>
            </div>
            
            <!-- Camera Stream Card -->
            <div class="flex-1 bg-white rounded-2xl shadow-md p-6 flex flex-col gap-4 min-w-[320px] hover:shadow-lg transition-all duration-300">
                <div class="flex items-center gap-4 mb-2">
                    <i class="fa fa-video text-green-500 text-3xl bg-green-100 rounded-full border-2 border-green-200 shadow p-4"></i>
                    <div>
                        <div class="text-lg font-bold text-gray-800">Camera Stream</div>
                        <div class="text-xs text-gray-500">Live Plant Monitoring</div>
                    </div>
                </div>
                <div class="flex justify-center mt-2">
                    <img id="camera-stream" src="" alt="Camera Stream" class="rounded-xl h-48 object-contain border border-gray-200 bg-gray-50">
                </div>
            </div>
        </div>
        </main>
</div>

<!-- Modal Dialog -->
<div id="modal" class="fixed inset-0 flex items-center justify-center z-50 hidden">
    <div class="absolute inset-0 bg-black bg-opacity-50"></div>
    <div class="bg-white rounded-lg shadow-xl p-6 w-11/12 max-w-2xl max-h-[80vh] overflow-y-auto z-10 relative">
        <div class="flex justify-between items-center mb-4">
            <h3 id="modal-title" class="text-xl font-bold text-gray-800">Report</h3>
            <button onclick="closeModal()" class="text-gray-500 hover:text-gray-700">
                <i class="fa fa-times"></i>
            </button>
        </div>
        <div id="modal-content" class="text-gray-700 text-sm whitespace-pre-wrap"></div>
        <div id="modal-loading" class="hidden">
            <div class="flex items-center justify-center py-4">
                <i class="fa fa-spinner fa-spin text-[#537D5D] text-2xl mr-2"></i>
                <span class="text-gray-700">Loading results...</span>
            </div>
        </div>
        <div class="mt-6 flex justify-end">
            <button onclick="closeModal()" class="bg-[#537D5D] text-white px-4 py-2 rounded-lg hover:bg-green-700 transition-all duration-300">Close</button>
        </div>
    </div>
</div>

    <script>
const API_URL = window.location.origin;
    function updateDateTime() {
            const now = new Date();
        document.getElementById('current-date').textContent = now.toLocaleDateString('en-GB', { weekday: 'long', day: '2-digit', month: '2-digit', year: 'numeric' });
        document.getElementById('current-time').textContent = now.toLocaleTimeString('en-GB');
    }
    setInterval(updateDateTime, 1000); updateDateTime();

// Định nghĩa API endpoint để đảm bảo nhất quán

// Hàm hiển thị thông báo
function showNotification(message, type = 'success') {
    const notificationArea = document.getElementById('notification-area');
    const notificationMessage = document.getElementById('notification-message');
    
    // Set the message
    notificationMessage.textContent = message;
    
    // Get the notification div (parent of the message)
    const notificationDiv = notificationMessage.parentElement;
    
    // Reset all classes
    notificationDiv.className = 'w-full px-4 py-3 rounded-lg shadow-md flex items-center transition-all duration-300';
    
    // Set the appropriate class based on type
    if (type === 'success') {
        notificationDiv.classList.add('bg-[#537D5D]', 'text-white');
        notificationMessage.previousElementSibling.className = 'fa fa-check-circle mr-2';
    } else if (type === 'error') {
        notificationDiv.classList.add('bg-red-500', 'text-white');
        notificationMessage.previousElementSibling.className = 'fa fa-exclamation-circle mr-2';
    } else if (type === 'info') {
        notificationDiv.classList.add('bg-blue-500', 'text-white');
        notificationMessage.previousElementSibling.className = 'fa fa-info-circle mr-2';
    }
    
    // Show the notification
    notificationArea.classList.remove('hidden');
    
    // Set a timeout to hide the notification after 5 seconds
    setTimeout(hideNotification, 5000);
}

// Hàm ẩn thông báo
function hideNotification() {
    document.getElementById('notification-area').classList.add('hidden');
}

    function setBar(barId, percent, valueId, percentId, value, unit) {
        const bar = document.getElementById(barId);
        const valueElem = document.getElementById(valueId);
        const percentElem = document.getElementById(percentId);
        
        // Handle undefined or null values
        if (value === undefined || value === null) {
            value = '--';
            percent = 0;
        }
        
        if (bar) {
            bar.style.width = percent + '%';
        
            // Xử lý màu dựa trên giá trị cảm biến
            let color;
            if (barId === 'temperature-bar') {
                // Nhiệt độ: xanh khi lạnh -> vàng -> đỏ khi nóng
                if (value < 18) {
                    color = '#3b82f6'; // xanh dương
                } else if (value < 22) {
                    color = '#60a5fa'; // xanh dương nhạt
                } else if (value < 26) {
                    color = '#10b981'; // xanh lá
                } else if (value < 30) {
                    color = '#facc15'; // vàng
                } else if (value < 34) {
                    color = '#f97316'; // cam
                } else {
                    color = '#ef4444'; // đỏ
                }
            } 
            else if (barId === 'humidity-bar') {
                // Độ ẩm không khí: đỏ khi khô -> vàng -> xanh khi ẩm
                if (percent < 30) {
                    color = '#ef4444'; // đỏ - quá khô
                } else if (percent < 45) {
                    color = '#f97316'; // cam - hơi khô
                } else if (percent < 60) {
                    color = '#10b981'; // xanh lá - tốt
                } else if (percent < 75) {
                    color = '#60a5fa'; // xanh dương nhạt - hơi ẩm
                } else {
                    color = '#3b82f6'; // xanh dương - quá ẩm
                }
            }
            else if (barId === 'soil-moisture-bar') {
                // Độ ẩm đất: đỏ khi khô -> vàng -> xanh khi ẩm
                if (percent < 20) {
                    color = '#ef4444'; // đỏ - quá khô
                } else if (percent < 35) {
                    color = '#f97316'; // cam - hơi khô
                } else if (percent < 60) {
                    color = '#10b981'; // xanh lá - tốt
                } else if (percent < 80) {
                    color = '#60a5fa'; // xanh dương nhạt - hơi ẩm
                } else {
                    color = '#3b82f6'; // xanh dương - quá ẩm
                }
            }
            
            bar.style.backgroundColor = color;
        }
        
        if (valueElem) {
            const oldValue = valueElem.textContent;
            valueElem.textContent = value;
            // Add animation if value has changed
            if (oldValue !== value.toString()) {
                animateStatusChange(valueElem);
            }
        }
        
        if (percentElem) {
            if (value === '--') {
                percentElem.textContent = '--';
            } else if (unit === '°C') {
                // Handle temperature display
                percentElem.textContent = value + unit;
            } else {
                // Handle percentage displays
                percentElem.textContent = percent + '%';
            }
        }
    }
    
    // Function to add highlight animation to updated elements
    function animateStatusChange(element) {
        // Remove the class if it's already there
        element.classList.remove('status-change');
        
        // Force a reflow to restart the animation
        void element.offsetWidth;
        
        // Add the class back to trigger the animation
        element.classList.add('status-change');
        
        // Remove class after animation completes
        setTimeout(() => {
            element.classList.remove('status-change');
        }, 2000);
    }

// Add spinner to button
function addSpinner(button) {
    // Save original content
    button.dataset.originalContent = button.innerHTML;
    // Replace with spinner
    const icon = button.querySelector('i') || document.createElement('i');
    icon.className = 'fa fa-spinner fa-spin mr-1';
    button.innerHTML = '';
    button.appendChild(icon);
    button.appendChild(document.createTextNode(' Đang cập nhật...'));
    button.disabled = true;
}

// Restore button state
function removeSpinner(button) {
    if (button.dataset.originalContent) {
        button.innerHTML = button.dataset.originalContent;
        button.disabled = false;
    }
    }

    function updateStatus() {
    return fetch(`${API_URL}/update`)
        .then(r => {
            if (!r.ok) {
                throw new Error(`HTTP error! Status: ${r.status}`);
            }
            return r.json();
        })
        .then(data => {
            // Air Humidity
            setBar('humidity-bar', data.humidity || 0, 'humidity-value', 'humidity-percent', data.humidity, '%');
            // Soil Moisture
            setBar('soil-moisture-bar', data.soil_moisture || 0, 'soil-moisture-value', 'soil-moisture-percent', data.soil_moisture, '%');
            // Temperature
            let tempPercent = data.temperature ? Math.min(100, Math.max(0, (data.temperature - 10) * 3.33)) : 0; // scale 10-40C to 0-100%
            setBar('temperature-bar', tempPercent, 'temperature-value', 'temperature-percent', data.temperature, '°C');
            
            // Update pump status with visual indicator
            const pumpStatus = data.pump_status === 'on' ? 'Running' : 'Off';
            document.getElementById('pump-status').textContent = pumpStatus;
            
            // Update pump icon based on status
            const pumpIcon = document.getElementById('pump-icon');
            if (data.pump_status === 'on') {
                pumpIcon.className = 'fa fa-power-off text-blue-500 text-2xl';
                pumpIcon.classList.add('animate-pulse');
            } else {
                pumpIcon.className = 'fa fa-power-off text-gray-400 text-2xl';
                pumpIcon.classList.remove('animate-pulse');
            }
            
            // Update watering schedule with better formatting
            document.getElementById('watering-schedule').textContent = data.next_watering_time ? `Next watering: ${data.next_watering_time}` : 'No schedule set';
            
            return data;
        })
        .catch(error => {
            console.error('Error updating status:', error);
            showNotification('Error updating data: ' + error.message, 'error');
            throw error;
        });
}

// We don't call updateStatus automatically anymore, only on button click
// Initial data loading to show something when the page loads
updateStatus();

// First update button - Just update local display data
    document.getElementById('update-data').onclick = function() {
    const button = this;
    addSpinner(button);
    
    updateStatus()
        .then(() => {
            showNotification('Data updated successfully!', 'success');
        })
        .catch(() => {
            showNotification('Error updating data!', 'error');
        })
        .finally(() => {
            removeSpinner(button);
        });
};

// Second update button - Send data to Google Sheets
document.getElementById('update-sheets').onclick = function() {
    const button = this;
    addSpinner(button);
    
    fetch(`${API_URL}/update-to-sheets`)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.json();
        })
        .then(() => {
            showNotification('Data sent to Google Sheets successfully!', 'success');
            return updateStatus();
        })
        .catch(err => {
            showNotification('Error sending data to Google Sheets: ' + err.message, 'error');
        })
        .finally(() => {
            removeSpinner(button);
        });
};

// Add modal functions
function showModal(title, content) {
    document.getElementById('modal-title').textContent = title;
    document.getElementById('modal-content').textContent = content;
    document.getElementById('modal').classList.remove('hidden');
    document.getElementById('modal-loading').classList.add('hidden');
    // Prevent body scrolling when modal is open
    document.body.style.overflow = 'hidden';
}

function showModalWithHTML(title, htmlContent) {
    document.getElementById('modal-title').textContent = title;
    document.getElementById('modal-content').innerHTML = htmlContent;
    document.getElementById('modal').classList.remove('hidden');
    document.getElementById('modal-loading').classList.add('hidden');
    // Prevent body scrolling when modal is open
    document.body.style.overflow = 'hidden';
}

function closeModal() {
    document.getElementById('modal').classList.add('hidden');
    // Re-enable body scrolling
    document.body.style.overflow = 'auto';
    // Clear any auto-check intervals
    if (window.analysisCheckInterval) {
        clearInterval(window.analysisCheckInterval);
        window.analysisCheckInterval = null;
    }
    if (window.reportCheckInterval) {
        clearInterval(window.reportCheckInterval);
        window.reportCheckInterval = null;
    }
}

// Function to format analysis text for better presentation
function formatAnalysisOutput(text) {
    if (!text) return '<p>No results available</p>';
    
    // Replace line breaks with HTML line breaks
    let html = text.replace(/\n/g, '<br>');
    
    // Highlight sections (headers with emojis)
    html = html.replace(/📊 ([^<]+)/g, '<h3 class="text-lg font-bold text-[#537D5D] mt-4 mb-2">📊 $1</h3>');
    html = html.replace(/🌡️ ([^<]+)/g, '<h4 class="font-bold text-[#537D5D] mt-3 mb-1">🌡️ $1</h4>');
    html = html.replace(/💧 ([^<]+)/g, '<h4 class="font-bold text-[#537D5D] mt-3 mb-1">💧 $1</h4>');
    html = html.replace(/🌱 ([^<]+)/g, '<h4 class="font-bold text-[#537D5D] mt-3 mb-1">🌱 $1</h4>');
    html = html.replace(/📝 ([^<]+)/g, '<h4 class="font-bold text-[#537D5D] mt-3 mb-1">📝 $1</h4>');
    html = html.replace(/💡 ([^<]+)/g, '<h4 class="font-bold text-[#537D5D] mt-3 mb-1">💡 $1</h4>');
    html = html.replace(/⚠️ ([^<]+)/g, '<div class="text-orange-500 mt-2">⚠️ $1</div>');
    html = html.replace(/✅ ([^<]+)/g, '<div class="text-green-600 mt-2">✅ $1</div>');
    html = html.replace(/🔮 ([^<]+)/g, '<h4 class="font-bold text-[#537D5D] mt-3 mb-1">🔮 $1</h4>');
    
    // Format bullets
    html = html.replace(/•(.*?)(?=<br>|$)/g, '<li class="ml-5">$1</li>');
    
    return html;
}

    document.getElementById('get-report').onclick = function() {
    const button = this;
    addSpinner(button);
    
    fetch(`${API_URL}/report`)
        .then(r => {
            if (!r.ok) {
                throw new Error(`HTTP error! Status: ${r.status}`);
            }
            return r.text();
        })
        .then(txt => {
            document.getElementById('report-data').innerHTML = `<p>Report received! Click to view details.</p>`;
            showNotification('Daily report request sent successfully!', 'success');
            showModalWithHTML('Daily Report Request', formatAnalysisOutput(txt));
            
            // Add a note about checking for results
            const modalContent = document.getElementById('modal-content');
            const checkResultsBtn = document.createElement('button');
            checkResultsBtn.className = 'mt-4 bg-[#537D5D] text-white px-4 py-2 rounded-lg hover:bg-green-700 transition-all duration-300 w-full';
            checkResultsBtn.innerHTML = '<i class="fa fa-refresh mr-2"></i>Check Report Results';
            
            // Auto-check for results every 5 seconds
            let checkCount = 0;
            
            function checkResults() {
                document.getElementById('modal-loading').classList.remove('hidden');
                
                // Call the new endpoint to get report results
                fetch(`${API_URL}/report-results`)
                    .then(r => {
                        if (!r.ok) {
                            throw new Error(`HTTP error! Status: ${r.status}`);
                        }
                        return r.text();
                    })
                    .then(resultTxt => {
                        document.getElementById('modal-loading').classList.add('hidden');
                        
                        if (resultTxt.includes("No report results available yet")) {
                            // Still waiting
                            checkResultsBtn.innerHTML = '<i class="fa fa-refresh mr-2"></i>Check Report Results';
                            checkResultsBtn.disabled = false;
                            
                            // Update the modal with the status
                            const statusLine = document.createElement('p');
                            statusLine.className = 'text-orange-500 mt-2';
                            statusLine.innerHTML = `⏳ Still processing... Check ${checkCount > 0 ? 'again ' : ''}in a few seconds`;
                            
                            // Only add status line if it doesn't exist already
                            const existingStatus = modalContent.querySelector('.text-orange-500');
                            if (existingStatus) {
                                existingStatus.remove();
                            }
                            
                            modalContent.appendChild(statusLine);
                            checkCount++;
                            
                            // After 3 checks, cancel auto-checking
                            if (checkCount >= 6) {
                                if (window.reportCheckInterval) {
                                    clearInterval(window.reportCheckInterval);
                                    window.reportCheckInterval = null;
                                    
                                    const timeoutNote = document.createElement('p');
                                    timeoutNote.className = 'text-red-500 mt-4';
                                    timeoutNote.innerHTML = 'Auto-checking stopped. Report generation might be taking longer than expected. Check Telegram for results or try again manually.';
                                    modalContent.appendChild(timeoutNote);
                                }
                            }
                        } else {
                            // Results are ready! Show a new modal with formatted results
                            closeModal();
                            setTimeout(() => {
                                showModalWithHTML('Daily Report', formatAnalysisOutput(resultTxt));
                                showNotification('Report results are ready!', 'success');
                                
                                // Clear the interval if it exists
                                if (window.reportCheckInterval) {
                                    clearInterval(window.reportCheckInterval);
                                    window.reportCheckInterval = null;
                                }
                            }, 300);
                        }
                    })
                    .catch(err => {
                        document.getElementById('modal-loading').classList.add('hidden');
                        checkResultsBtn.innerHTML = '<i class="fa fa-refresh mr-2"></i>Check Report Results';
                        checkResultsBtn.disabled = false;
                        showNotification('Error checking report results: ' + err.message, 'error');
                        
                        // Clear the interval if it exists
                        if (window.reportCheckInterval) {
                            clearInterval(window.reportCheckInterval);
                            window.reportCheckInterval = null;
                        }
                    });
            }
            
            // Function for manual check
            checkResultsBtn.onclick = function() {
                this.innerHTML = '<i class="fa fa-spinner fa-spin mr-2"></i>Checking for results...';
                this.disabled = true;
                checkResults();
            };
            
            // Add the button to the modal
            modalContent.appendChild(checkResultsBtn);
            
            // Set up auto-checking every 5 seconds
            if (window.reportCheckInterval) {
                clearInterval(window.reportCheckInterval);
            }
            window.reportCheckInterval = setInterval(checkResults, 5000);
            
            // Add note about auto-checking
            const autoCheckNote = document.createElement('p');
            autoCheckNote.className = 'text-xs text-gray-500 mt-2 italic';
            autoCheckNote.textContent = 'Automatically checking for results every 5 seconds...';
            modalContent.appendChild(autoCheckNote);
        })
        .catch(err => {
            showNotification('Error requesting report: ' + err.message, 'error');
        })
        .finally(() => {
            removeSpinner(button);
        });
    };

    document.getElementById('get-analysis').onclick = function() {
    const button = this;
    addSpinner(button);
    
    fetch(`${API_URL}/analysis`)
        .then(r => {
            if (!r.ok) {
                throw new Error(`HTTP error! Status: ${r.status}`);
            }
            return r.text();
        })
        .then(txt => {
            showNotification('Detailed analysis request sent successfully!', 'success');
            showModal('Detailed Analysis Request', txt);
            
            // Add a note about checking for results
            const modalContent = document.getElementById('modal-content');
            const checkResultsBtn = document.createElement('button');
            checkResultsBtn.className = 'mt-4 bg-[#537D5D] text-white px-4 py-2 rounded-lg hover:bg-green-700 transition-all duration-300 w-full';
            checkResultsBtn.innerHTML = '<i class="fa fa-refresh mr-2"></i>Check Analysis Results';
            
            // Auto-check for results every 5 seconds
            let checkCount = 0;
            
            function checkResults() {
                document.getElementById('modal-loading').classList.remove('hidden');
                
                // Call the new endpoint to get analysis results
                fetch(`${API_URL}/analysis-results`)
                    .then(r => {
                        if (!r.ok) {
                            throw new Error(`HTTP error! Status: ${r.status}`);
                        }
                        return r.text();
                    })
                    .then(resultTxt => {
                        document.getElementById('modal-loading').classList.add('hidden');
                        
                        if (resultTxt.includes("No analysis results available yet")) {
                            // Still waiting
                            checkResultsBtn.innerHTML = '<i class="fa fa-refresh mr-2"></i>Check Analysis Results';
                            checkResultsBtn.disabled = false;
                            
                            // Update the modal with the status
                            const statusLine = document.createElement('p');
                            statusLine.className = 'text-orange-500 mt-2';
                            statusLine.innerHTML = `⏳ Still processing... Check ${checkCount > 0 ? 'again ' : ''}in a few seconds`;
                            
                            // Only add status line if it doesn't exist already
                            const existingStatus = modalContent.querySelector('.text-orange-500');
                            if (existingStatus) {
                                existingStatus.remove();
                            }
                            
                            modalContent.appendChild(statusLine);
                            checkCount++;
                            
                            // After 3 checks, cancel auto-checking
                            if (checkCount >= 6) {
                                if (window.analysisCheckInterval) {
                                    clearInterval(window.analysisCheckInterval);
                                    window.analysisCheckInterval = null;
                                    
                                    const timeoutNote = document.createElement('p');
                                    timeoutNote.className = 'text-red-500 mt-4';
                                    timeoutNote.innerHTML = 'Auto-checking stopped. Analysis might be taking longer than expected. Check Telegram for results or try again manually.';
                                    modalContent.appendChild(timeoutNote);
                                }
                            }
                        } else {
                            // Results are ready! Show a new modal with formatted results
                            closeModal();
                            setTimeout(() => {
                                showModalWithHTML('Detailed Analysis Results', formatAnalysisOutput(resultTxt));
                                showNotification('Analysis results are ready!', 'success');
                                
                                // Clear the interval if it exists
                                if (window.analysisCheckInterval) {
                                    clearInterval(window.analysisCheckInterval);
                                    window.analysisCheckInterval = null;
                                }
                            }, 300);
                        }
                    })
                    .catch(err => {
                        document.getElementById('modal-loading').classList.add('hidden');
                        checkResultsBtn.innerHTML = '<i class="fa fa-refresh mr-2"></i>Check Analysis Results';
                        checkResultsBtn.disabled = false;
                        showNotification('Error checking analysis results: ' + err.message, 'error');
                        
                        // Clear the interval if it exists
                        if (window.analysisCheckInterval) {
                            clearInterval(window.analysisCheckInterval);
                            window.analysisCheckInterval = null;
                        }
                    });
            }
            
            // Function for manual check
            checkResultsBtn.onclick = function() {
                this.innerHTML = '<i class="fa fa-spinner fa-spin mr-2"></i>Checking for results...';
                this.disabled = true;
                checkResults();
            };
            
            // Add the button to the modal
            modalContent.appendChild(checkResultsBtn);
            
            // Set up auto-checking every 5 seconds
            if (window.analysisCheckInterval) {
                clearInterval(window.analysisCheckInterval);
            }
            window.analysisCheckInterval = setInterval(checkResults, 5000);
            
            // Add note about auto-checking
            const autoCheckNote = document.createElement('p');
            autoCheckNote.className = 'text-xs text-gray-500 mt-2 italic';
            autoCheckNote.textContent = 'Automatically checking for results every 5 seconds...';
            modalContent.appendChild(autoCheckNote);
        })
        .catch(err => {
            showNotification('Error requesting analysis: ' + err.message, 'error');
        })
        .finally(() => {
            removeSpinner(button);
        });
    };

    document.getElementById('start-watering').onclick = function() {
    const button = this;
    addSpinner(button);
    
    // Call the startWaterPump function endpoint
    fetch(`${API_URL}/startWaterPump`, {method: 'POST'})
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response;
        })
        .then(() => {
            // Immediately update UI to show pump is running
            const pumpStatus = document.getElementById('pump-status');
            const pumpIcon = document.getElementById('pump-icon');
            
            pumpStatus.textContent = 'Running';
            pumpStatus.classList.add('text-blue-500');
            animateStatusChange(pumpStatus);
            
            pumpIcon.className = 'fa fa-power-off text-blue-500 text-2xl';
            pumpIcon.classList.add('animate-pulse');
            
            showNotification('Watering started and will run for 10 seconds!', 'success');
            
            // Update UI after watering completes (10 seconds)
            setTimeout(() => {
                pumpStatus.textContent = 'Off';
                pumpStatus.classList.remove('text-blue-500');
                animateStatusChange(pumpStatus);
                
                pumpIcon.className = 'fa fa-power-off text-gray-400 text-2xl';
                pumpIcon.classList.remove('animate-pulse');
                
                showNotification('Watering completed!', 'success');
            }, 10500); // Add extra 500ms buffer
            
            return updateStatus();
        })
        .catch(err => {
            showNotification('Error starting watering: ' + err.message, 'error');
        })
        .finally(() => {
            removeSpinner(button);
        });
};

    document.getElementById('set-watering-time').onclick = function() {
    const button = this;
        const selectedTime = document.getElementById('watering-time').value;
    
    if (!selectedTime) {
        showNotification('Please select a watering time!', 'error');
        return;
    }
    
    addSpinner(button);
    
    // Call endpoint to set scheduledWateringTime
    fetch(`${API_URL}/setScheduledWateringTime?time=${encodeURIComponent(selectedTime)}`, {method: 'POST'})
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response;
        })
        .then(() => {
            // Immediately update UI to show new schedule
            const scheduleElement = document.getElementById('watering-schedule');
            scheduleElement.textContent = `Next watering: ${selectedTime}`;
            animateStatusChange(scheduleElement);
            
            showNotification(`Watering schedule set for ${selectedTime}`, 'success');
            return updateStatus();
        })
        .catch(err => {
            showNotification('Error setting watering schedule: ' + err.message, 'error');
        })
        .finally(() => {
            removeSpinner(button);
        });
};

    function updateWeather() {
    fetch(`${API_URL}/weather`)
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! Status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            document.getElementById('weather-icon').className = 'fa fa-' + weatherIcons[data.icon] || 'fa fa-sun-o';
            document.getElementById('weather-temp').textContent = data.temperature;
            document.getElementById('weather-desc').textContent = data.description;
            document.getElementById('weather-location').textContent = data.location;
            document.getElementById('weather-update-time').textContent = data.lastUpdated;
        })
        .catch(error => {
            console.error('Error fetching weather data:', error);
            showNotification('Error fetching weather data: ' + error.message, 'error');
        });
    }
    updateWeather(); setInterval(updateWeather, 300000);

    // Disease Prediction API
    const predictBtn = document.getElementById('predict-btn');
    if (predictBtn) {
        predictBtn.onclick = function() {
        const button = this;
        addSpinner(button);
        
        fetch(`http://192.168.1.65/predict`)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP error! Status: ${response.status}`);
                }
                return response.json();
            })
            .then(data => {
                // Update prediction result with better formatting
                document.getElementById('predicted-class').textContent = data.predicted_class || '--';
                document.getElementById('predicted-confidence').textContent = data.confidence || '--';
                document.getElementById('possible-disease').textContent = data.disease || '--';
                
                // Apply styling based on prediction result
                const statusElement = document.getElementById('predicted-class');
                if (data.predicted_class) {
                    if (data.predicted_class.toLowerCase().includes('healthy')) {
                        statusElement.className = 'font-bold text-green-600';
                    } else {
                        statusElement.className = 'font-bold text-red-600';
                    }
                    
                    // Send data to the main ESP32 to update the disease status
                    fetch(`${API_URL}/receive-disease`, {
                        method: 'POST',
                        headers: {
                            'Content-Type': 'application/x-www-form-urlencoded',
                        },
                        body: `predicted_class=${data.predicted_class}`
                    })
                    .then(response => response.json())
                    .then(result => {
                        console.log('Disease data sent to main ESP32:', result);
                    })
                    .catch(error => {
                        console.error('Error sending disease data to main ESP32:', error);
                    });
                }
                
                // Additional metadata if available
                if (data.metadata) {
                    document.getElementById('predict-meta').innerHTML = `<div class="mt-3 pt-2 border-t border-gray-200">
                        <span class="text-xs font-semibold text-gray-500">Additional Information</span>
                        <p class="text-xs mt-1">${data.metadata}</p>
                    </div>`;
                } else {
                    document.getElementById('predict-meta').innerHTML = '';
                }
                
                // Show probabilities if available
                if (data.probabilities) {
                    let probHtml = `<div class="mt-3 pt-2 border-t border-gray-200">
                        <span class="text-xs font-semibold text-gray-500">Class Probabilities</span>
                        <div class="grid grid-cols-2 gap-1 mt-1">`;
                    
                    for (const [cls, prob] of Object.entries(data.probabilities)) {
                        probHtml += `<div class="text-xs flex justify-between">
                            <span>${cls}:</span>
                            <span class="font-semibold">${prob}%</span>
                        </div>`;
                    }
                    
                    probHtml += `</div></div>`;
                    document.getElementById('all-probabilities').innerHTML = probHtml;
                } else {
                    document.getElementById('all-probabilities').innerHTML = '';
                }
                
                // Show image if available
                if (data.image_base64) {
                    document.getElementById('predict-image').innerHTML = `
                        <div class="mt-3 pt-2 border-t border-gray-200">
                            <span class="text-xs font-semibold text-gray-500">Captured Image</span>
                            <div class="flex justify-center mt-2">
                                <img src="data:image/jpeg;base64,${data.image_base64}" class="max-w-full h-auto max-h-36 rounded border border-gray-200" />
                            </div>
                        </div>`;
                } else {
                    document.getElementById('predict-image').innerHTML = '';
                }
                
                showNotification('Disease prediction completed!', 'success');
            })
            .catch(err => {
                showNotification('Error making prediction: ' + err.message, 'error');
            })
            .finally(() => {
                removeSpinner(button);
            });
        };
    }
    
    // Device Status API
    const statusBtn = document.getElementById('status-btn');
    if (statusBtn) {
        statusBtn.onclick = function() {
            const button = this;
            addSpinner(button);
            
            fetch(`http://192.168.1.65/status`)
                .then(r => {
                    if (!r.ok) {
                        throw new Error(`HTTP error! Status: ${r.status}`);
                    }
                    return r.json();
                })
                .then(data => {
                    // Update status with better formatting
                    document.getElementById('device-status').textContent = data.status || 'Online';
                    
                    // Update device information with icons
                    document.getElementById('device-ip').textContent = data.ip || '--';
                    document.getElementById('device-ssid').textContent = data.ssid || '--';
                    document.getElementById('device-time').textContent = data.time || '--';
                    document.getElementById('device-date').textContent = data.date || '--';
                    document.getElementById('device-uptime').textContent = data.uptime || '--';
                    document.getElementById('device-freeheap').textContent = data.free_heap || '--';
                    document.getElementById('device-apiurl').textContent = data.api_url || '--';
                    
                    // Apply conditional styling based on status
                    const statusElement = document.getElementById('device-status');
                    if (data.status && data.status.toLowerCase().includes('online')) {
                        statusElement.className = 'font-bold text-green-600';
                    } else if (data.status && data.status.toLowerCase().includes('error')) {
                        statusElement.className = 'font-bold text-red-600';
                    } else {
                        statusElement.className = 'font-bold text-orange-500';
                    }
                    
                    showNotification('Device status updated!', 'success');
                })
                .catch(err => {
                    showNotification('Error getting device status: ' + err.message, 'error');
                })
                .finally(() => {
                    removeSpinner(button);
                });
        };
    }
    
    // Capture Image API
    const captureBtn = document.getElementById('capture-btn');
    if (captureBtn) {
        captureBtn.onclick = function() {
            // Reload the image to get the latest capture
            const img = document.getElementById('capture-image');
            img.src = `http://192.168.1.65/capture-image?ts=${Date.now()}`;
        };
    }
    
    // Cập nhật đường dẫn ảnh khi tải trang
    document.addEventListener('DOMContentLoaded', function() {
        // Cập nhật hình ảnh theo API_URL
        document.getElementById('capture-image').src = `http://192.168.1.65/capture-image`;
        document.getElementById('camera-stream').src = `http://192.168.1.65/stream`;
    });
    </script>
</body>
</html>
)html";