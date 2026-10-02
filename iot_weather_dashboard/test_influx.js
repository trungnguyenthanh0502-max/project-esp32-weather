import dotenv from 'dotenv';
import { InfluxDB, Point } from '@influxdata/influxdb-client';

dotenv.config();

const url = process.env.INFLUX_URL;
const token = process.env.INFLUX_TOKEN;
const org = process.env.INFLUX_ORG;
const bucket = process.env.INFLUX_BUCKET;

const influxDB = new InfluxDB({ url, token });

// 1. Ghi dữ liệu test
async function writeTestData() {
  const writeApi = influxDB.getWriteApi(org, bucket);
  
  // Lưu ý: Cập nhật field name khớp với server.js mới (mqtt_consumer & temperature_dht)
  const point = new Point('mqtt_consumer')
    .floatField('temperature_dht', 28.5)
    .floatField('humidity', 60.0)
    .floatField('pressure', 1012.0)
    .floatField('weather_code', 1);

  writeApi.writePoint(point);

  try {
    await writeApi.close();
    console.log('✅ Ghi dữ liệu mẫu vào InfluxDB thành công!');
  } catch (e) {
    console.error('❌ Lỗi khi ghi dữ liệu:', e);
  }
}

// 2. Đọc dữ liệu test
async function readTestData() {
  const queryApi = influxDB.getQueryApi(org);
  const fluxQuery = `
    from(bucket: "${bucket}")
      |> range(start: -1h)
      |> filter(fn: (r) => r._measurement == "mqtt_consumer")
  `;

  console.log('🔍 Đang truy vấn InfluxDB...');
  
  try {
    queryApi.queryRows(fluxQuery, {
      next(row, tableMeta) {
        const o = tableMeta.toObject(row);
        console.log(`[Data] ${o._time} | Field: ${o._field} = ${o._value}`);
      },
      error(error) {
        console.error('❌ Lỗi truy vấn:', error);
      },
      complete() {
        console.log('✅ Đọc dữ liệu thành công!');
      },
    });
  } catch (e) {
    console.error('❌ Lỗi kết nối Query API:', e);
  }
}

async function runTest() {
  await writeTestData();
  setTimeout(readTestData, 1000);
}

runTest();