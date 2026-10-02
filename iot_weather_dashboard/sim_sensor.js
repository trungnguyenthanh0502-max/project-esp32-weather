import dotenv from 'dotenv';
import { InfluxDB, Point } from '@influxdata/influxdb-client';

dotenv.config();

const influxDB = new InfluxDB({ 
  url: process.env.INFLUX_URL, 
  token: process.env.INFLUX_TOKEN 
});

const writeApi = influxDB.getWriteApi(process.env.INFLUX_ORG, process.env.INFLUX_BUCKET);

function getRandom(min, max) {
  return parseFloat((Math.random() * (max - min) + min).toFixed(1));
}

function sendData() {
  const temp = getRandom(24.0, 32.0);
  const hum = getRandom(40.0, 85.0);
  const pres = getRandom(1005.0, 1015.0);
  const weatherCode = Math.floor(getRandom(0, 3)); // 0, 1, 2

  // Measurement đổi thành mqtt_consumer & temp field thành temperature_dht
  const point = new Point('mqtt_consumer')
    .floatField('temperature_dht', temp)
    .floatField('humidity', hum)
    .floatField('pressure', pres)
    .floatField('weather_code', weatherCode);

  writeApi.writePoint(point);
  
  writeApi.flush()
    .then(() => {
      console.log(`[${new Date().toLocaleTimeString()}] 🚀 Đã gửi: Temp=${temp}°C, Hum=${hum}%, Pres=${pres}hPa`);
    })
    .catch(err => {
      console.error('❌ Lỗi gửi data:', err);
    });
}

console.log('📡 Bắt đầu gửi dữ liệu cảm biến (mô phỏng)...');
sendData();
setInterval(sendData, 2000); // Gửi mỗi 2 giây