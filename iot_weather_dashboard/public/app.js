let chartInstance = null;
let currentRange = '6h';

// Map mã thời tiết Open-Meteo sang Icon & chữ Tiếng Việt
function decodeWeather(code) {
  if (code === 0) return { icon: '☀️', text: 'Nắng quang' };
  if ([1, 2, 3].includes(code)) return { icon: '⛅', text: 'Có mây' };
  if ([45, 48].includes(code)) return { icon: '🌫️', text: 'Sương mù' };
  if ([51, 61, 80].includes(code)) return { icon: '🌧️', text: 'Mưa nhẹ' };
  if ([63, 65, 81, 82].includes(code)) return { icon: '🌧️', text: 'Mưa to' };
  if ([95, 96, 99].includes(code)) return { icon: '🌩️', text: 'Dông bão' };
  return { icon: '🌤️', text: 'Nắng nhẹ' };
}

// Tính Emoji tâm trạng
function pickMood(temp, hum, pres) {
  if (!temp) return '😐';
  if (temp > 31) return '😫';
  if (temp >= 24 && temp <= 28 && hum >= 40 && hum <= 60) return '😊';
  return '🙁';
}

// Cập nhật Ngày tháng theo Tiếng Việt
function updateDateDisplay() {
  const days = ['Chủ Nhật', 'Thứ Hai', 'Thứ Ba', 'Thứ Tư', 'Thứ Năm', 'Thứ Sáu', 'Thứ Bảy'];
  const now = new Date();
  const dayName = days[now.getDay()];
  const dateStr = now.toLocaleDateString('vi-VN', { day: '2-digit', month: '2-digit', year: 'numeric' });
  document.getElementById('currentDate').innerText = `${dayName}, ${dateStr}`;
}

// 1. Vẽ / Cập nhật Chart.js
function renderChart(labels, tempData, humData) {
  const ctx = document.getElementById('weatherChart').getContext('2d');
  if (chartInstance) chartInstance.destroy();

  chartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: labels,
      datasets: [
        {
          label: 'Temperature (°C)',
          data: tempData,
          borderColor: '#ef4444',
          borderWidth: 2,
          pointRadius: 3,
          tension: 0.3,
          yAxisID: 'yTemp'
        },
        {
          label: 'Humidity (%)',
          data: humData,
          borderColor: '#2563eb',
          borderWidth: 2,
          pointRadius: 3,
          tension: 0.3,
          yAxisID: 'yHum'
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      scales: {
        yTemp: { type: 'linear', position: 'left', title: { display: true, text: 'Nhiệt độ (°C)' } },
        yHum: { type: 'linear', position: 'right', title: { display: true, text: 'Độ ẩm (%)' }, grid: { drawOnChartArea: false } }
      }
    }
  });
}

// 2. Fetch dữ liệu Realtime
async function fetchRealtime() {
  try {
    const res = await fetch('/api/realtime');
    const data = await res.json();

    if (data.temperature !== undefined) {
      document.getElementById('mainTemp').innerText = `${data.temperature.toFixed(1)}°C`;
      document.getElementById('humidityVal').innerText = `${data.humidity.toFixed(1)}%`;
      document.getElementById('pressureVal').innerText = `${data.pressure.toFixed(1)} hPa`;
      document.getElementById('moodIcon').innerText = pickMood(data.temperature, data.humidity, data.pressure);

      if (data.weather_code !== null) {
        const info = decodeWeather(data.weather_code);
        document.getElementById('weatherIcon').innerText = info.icon;
        document.getElementById('weatherText').innerText = info.text;
      }
    }
  } catch (err) {
    console.error('Lỗi realtime:', err);
  }
}

// 3. Fetch dữ liệu Metrics (Tự động cập nhật Min/Max)
async function fetchMetrics() {
  try {
    const res = await fetch(`/api/metrics?range=${currentRange}`);
    const data = await res.json();

    if (data.temperature && data.temperature.length > 0) {
      const minTemp = Math.min(...data.temperature).toFixed(1);
      const maxTemp = Math.max(...data.temperature).toFixed(1);
      document.getElementById('tempRange').innerText = `${minTemp} / ${maxTemp} °C`;

      const labels = data.time.map(t => new Date(t).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }));
      renderChart(labels, data.temperature, data.humidity);
    }
  } catch (err) {
    console.error('Lỗi metrics:', err);
  }
}

// 4. Fetch trạng thái ESP32
async function fetchEspStatus() {
  try {
    const res = await fetch('/api/esp32/status');
    const data = await res.json();
    const dot = document.getElementById('statusDot');
    const statusText = document.getElementById('espStatusText');

    if (data.connected) {
      dot.className = 'status-dot online';
      statusText.innerText = 'Connected (ESP32)';
    } else {
      dot.className = 'status-dot';
      statusText.innerText = 'Disconnected (ESP32)';
    }
  } catch (err) {
    console.error('Lỗi ESP status:', err);
  }
}

// Bắt sự kiện lọc khoảng thời gian
document.getElementById('timeRange')?.addEventListener('change', (e) => {
  currentRange = e.target.value;
  fetchMetrics();
});

// Đổi thành phố
document.getElementById('changeCityBtn')?.addEventListener('click', async () => {
  const city = prompt('Nhập tên thành phố:');
  if (city) {
    await fetch('/api/change_city', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ city })
    });
    document.getElementById('headerCity').innerText = city;
    document.getElementById('locationCity').innerText = city;
  }
});

// Sleep LCD
document.getElementById('sleepBtn')?.addEventListener('click', async () => {
  await fetch('/api/command/sleep', { method: 'POST' });
  alert('Đã gửi lệnh Sleep LCD!');
});

function init() {
  updateDateDisplay();
  fetchRealtime();
  fetchMetrics();
  fetchEspStatus();

  setInterval(fetchRealtime, 1000);
  setInterval(fetchMetrics, 3000);
  setInterval(fetchEspStatus, 5000);
}

init();