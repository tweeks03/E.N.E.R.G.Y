const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>E.N.E.R.G.Y Dashboard</title>
  <script src="https://code.highcharts.com/highcharts.js"></script>
  <style>
    body { font-family: sans-serif; padding: 20px; background: #f9f9f9; }
    .section-title { text-align: center; margin-top: 40px; font-size: 1.6em; color: #333; }
    .divider { border: none; border-top: 2px solid #ddd; margin: 30px auto; width: 80%; }
    .sensor-row { display: flex; justify-content: center; gap: 20px; margin-top: 20px; flex-wrap: wrap; }
    .sensor-row.single { justify-content: center; }
    .sensor-box { background: white; border: 4px solid #ccc; border-radius: 10px; padding: 16px; width: 220px; box-shadow: 2px 2px 8px rgba(0,0,0,0.1); transition: border-color 0.4s; }
    .sensor-box.main { border-color: #4CAF50; }
    .sensor-box.alt { border-color: #F44336; }
    .sensor-box h2 { margin-top: 0; font-size: 1.2em; }
    .switch-container { margin-top: 15px; text-align: center; }
    .switch { position: relative; display: inline-block; width: 50px; height: 24px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 24px; transition: 0.4s; }
    .slider:before { content: ""; position: absolute; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; border-radius: 50%; transition: 0.4s; }
    input:checked + .slider { background-color: #4CAF50; }
    input:checked + .slider:before { transform: translateX(26px); }
    .power-label { display: block; font-weight: bold; margin-top: 8px; }
    button {padding: 10px 20px; font-size: 1em; border: none; border-radius: 8px; background-color: #2196F3; color: white; cursor: pointer; transition: background-color 0.3s;}
    button:hover {background-color: #0b7dda;}
  </style>
</head>
<body>
  <h1>E.N.E.R.G.Y Dashboard</h1>

  <h2 class="section-title">Current History (All Sensors)</h2>
  <div id="chart-current" style="width:100%; height:400px; margin-bottom: 40px;"></div>

  <h2 class="section-title">Evans</h2>
  <div id="evansRow" class="sensor-row"></div>
  <hr class="divider">
  <h2 class="section-title">DuPont</h2>
  <div id="dupontRow" class="sensor-row"></div>
  <hr class="divider">
  <h2 class="section-title">Hospital</h2>
  <div id="hospitalRow" class="sensor-row single"></div>
  <hr class="divider">
  
  <h2 class="section-title">Simulation Controls</h2>
  <div style="display: flex; justify-content: center; gap: 20px; flex-wrap: wrap; margin-bottom: 40px;">
    <button onclick="runSim(1)">Reset</button>
    <button onclick="runSim(2)">Fault</button>
    <button onclick="runSim(3)">Daily Load</button>
    <button onclick="runSim(4)">Load Drop</button>
    <button onclick="runSim(5)">Load Ramp</button>
    <button onclick="runSim(6)">Solar Effect</button>
    <button onclick="runSim(7)">Demo</button>
    <button onclick="runSim(8)">Uniform Load</button>
  </div>

  <script>
    const labels = ["Evans 1", "Evans 2", "Evans 3", "DuPont 1", "DuPont 2", "DuPont 3", "Hospital"];

    // Create the Highcharts graph
    var chartCurrent = new Highcharts.Chart({
      chart: {
        renderTo: 'chart-current',
        type: 'spline' // smooth line
      },
      title: { text: 'Current History (All Sensors)' },
      xAxis: { type: 'datetime', title: { text: 'Time' } },
      yAxis: { title: { text: 'Current (mA)' } },
      series: [
        { name: 'Evans 1', data: [], color: 'green' },
        { name: 'Evans 2', data: [], color: 'blue' },
        { name: 'Evans 3', data: [], color: 'cyan' },
        { name: 'DuPont 1', data: [], color: 'red' },
        { name: 'DuPont 2', data: [], color: 'yellow' },
        { name: 'DuPont 3', data: [], color: 'purple' },
        { name: 'Hospital', data: [], color: 'black' }
      ]
    });

    async function fetchAndDisplayCSV() {
      try {
        const res = await fetch('/sensors?_t=' + new Date().getTime());
        const text = await res.text();
        const rows = text.trim().split(/\r?\n/).slice(1);
        const now = (new Date()).getTime();
        document.getElementById('evansRow').innerHTML = '';
        document.getElementById('dupontRow').innerHTML = '';
        document.getElementById('hospitalRow').innerHTML = '';
        rows.forEach((row, index) => {
          const [id, voltage, current, source] = row.split(',');
          const isAlt = source.trim().toLowerCase() === 'alternate';
          const card = document.createElement('div');
          card.className = `sensor-box ${isAlt ? 'alt' : 'main'}`;
          card.innerHTML = `
            <h2>${labels[index]}</h2>
            <p><strong>Voltage:</strong> ${voltage} V</p>
            <p><strong>Current:</strong> ${current} mA</p>
            <p><strong>Power Source:</strong> <span id="label-${index}">${source}</span></p>
            <div class="switch-container">
              <label class="switch">
                <input type="checkbox" id="toggle-${index}" ${isAlt ? 'checked' : ''} onchange="toggleSource(${index}, this)">
                <span class="slider"></span>
              </label>
              <span class="power-label">${isAlt ? 'Alternate' : 'Main'}</span>
            </div>
          `;
          if (index < 3) document.getElementById('evansRow').appendChild(card);
          else if (index < 6) document.getElementById('dupontRow').appendChild(card);
          else document.getElementById('hospitalRow').appendChild(card);

          // Update graph with new current value
          const current_mA = parseFloat(current);
          if (chartCurrent.series[index].data.length > 40) {
            chartCurrent.series[index].addPoint([now, current_mA], true, true, true);
          } else {
            chartCurrent.series[index].addPoint([now, current_mA], true, false, true);
          }
        });
      } catch (err) {
        console.error("Failed to fetch /sensors:", err);
      }
    }

    function toggleSource(index, checkbox) {
      const label = document.getElementById(`label-${index}`);
      const powerLabel = checkbox.closest('.switch-container').querySelector('.power-label');
      const card = checkbox.closest('.sensor-box');
      const newState = checkbox.checked ? 'Alternate' : 'Main';
      label.textContent = newState;
      powerLabel.textContent = newState;
      card.classList.remove('main', 'alt');
      card.classList.add(newState === 'Alternate' ? 'alt' : 'main');
      fetch('/toggle', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `index=${index}&state=${newState}`
      });
    }

    function runSim(simNumber) {
      fetch(`/sim?num=${simNumber}`)
        .then(res => res.text())
        .then(text => console.log("Response:", text))
        .catch(err => console.error("Sim error:", err));
    }

    fetchAndDisplayCSV();
    setInterval(fetchAndDisplayCSV, 1000);
  </script>
</body>
</html>
)rawliteral";
