import express from "express";
import dotenv from "dotenv";
import cors from "cors";
import { InfluxDB } from "@influxdata/influxdb-client";
import mqtt from "mqtt";
import path from "path";
import { fileURLToPath } from "url";

dotenv.config();
const app = express();
app.use(cors());
app.use(express.json());

// ===== MQTT Client =====
const MQTT_BROKER = process.env.MQTT_BROKER || "mqtt://localhost:1883";
const COMMAND_TOPIC = "iot/house/commands";
const mqttClient = mqtt.connect(MQTT_BROKER);
mqttClient.on("connect", () => console.log("✅ MQTT connected:", MQTT_BROKER));
mqttClient.on("error", (err) => console.error("❌ MQTT error:", err.message));

// ===== InfluxDB Config =====
const { INFLUX_URL, INFLUX_TOKEN, INFLUX_ORG, INFLUX_BUCKET, PORT } = process.env;
const influx = new InfluxDB({ url: INFLUX_URL, token: INFLUX_TOKEN });
const queryApi = influx.getQueryApi(INFLUX_ORG);

// ===== API: Dữ liệu realtime (0.5s) =====
app.get("/api/realtime", async (req, res) => {
  const fluxQuery = `
    from(bucket: "${INFLUX_BUCKET}")
      |> range(start: -1m)
      |> filter(fn: (r) => r["_measurement"] == "mqtt_consumer")
      |> filter(fn: (r) =>
          r["_field"] == "temperature_dht" or 
          r["_field"] == "humidity" or 
          r["_field"] == "pressure" or 
          r["_field"] == "weather_code")
      |> last()
  `;
  try {
    const rows = await queryApi.collectRows(fluxQuery);
    const latest = {};
    rows.forEach((r) => (latest[r._field] = r._value));

    res.json({
      temperature: latest.temperature_dht,
      humidity: latest.humidity,
      pressure: latest.pressure,
      weather_code: latest.weather_code !== undefined ? Math.round(latest.weather_code) : null,
      time: rows[0]?._time,
    });
  } catch (err) {
    console.error("❌ Lỗi realtime:", err.message);
    res.status(500).json({ error: "InfluxDB query failed" });
  }
});

// ===== API: Dữ liệu biểu đồ (đủ range thời gian) =====
app.get("/api/metrics", async (req, res) => {
  const range = req.query.range || "15m";

  // Auto chọn độ phân giải hợp lý theo range
  const windowMap = {
    "15m": "30s",
    "30m": "1m",
    "1h": "2m",
    "6h": "10m",
    "12h": "20m",
    "1d": "30m",
  };
  const every = windowMap[range] || "1m";

  const fluxQuery = `
    from(bucket: "${INFLUX_BUCKET}")
      |> range(start: -${range})
      |> filter(fn: (r) => r["_measurement"] == "mqtt_consumer")
      |> filter(fn: (r) =>
          r["_field"] == "temperature_dht" or 
          r["_field"] == "humidity" or 
          r["_field"] == "pressure" or 
          r["_field"] == "weather_code")
      |> aggregateWindow(every: ${every}, fn: last, createEmpty: false)
      |> sort(columns: ["_time"], desc: false)
  `;
  try {
    const rows = await queryApi.collectRows(fluxQuery);
    const result = { time: [], temperature: [], humidity: [], pressure: [], weather_code: [] };

    rows.forEach((r) => {
      const t = r._time;
      if (!result.time.includes(t)) result.time.push(t);
      switch (r._field) {
        case "temperature_dht": result.temperature.push(r._value); break;
        case "humidity": result.humidity.push(r._value); break;
        case "pressure": result.pressure.push(r._value); break;
        case "weather_code": result.weather_code.push(Math.round(r._value)); break;
      }
    });

    res.json(result);
  } catch (err) {
    console.error("❌ Lỗi metrics:", err.message);
    res.status(500).json({ error: "InfluxDB query failed" });
  }
});

// ===== LCD Commands =====
app.post("/api/command/sleep", (req, res) => {
  mqttClient.publish(COMMAND_TOPIC, "SLEEP");
  res.json({ ok: true });
});
app.post("/api/command/open", (req, res) => {
  mqttClient.publish(COMMAND_TOPIC, "OPEN");
  res.json({ ok: true });
});

// ===== Change City =====
app.post("/api/change_city", (req, res) => {
  const { city } = req.body;
  if (!city || city.trim() === "") return res.status(400).json({ error: "Thiếu tên thành phố" });
  const payload = JSON.stringify({ cmd: "change_city", city: city.trim() });
  mqttClient.publish(COMMAND_TOPIC, payload);
  console.log("📡 Gửi lệnh MQTT:", payload);
  res.json({ ok: true, message: `Đã gửi yêu cầu đổi vị trí: ${city}` });
});

// ===== Serve frontend =====
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
app.use(express.static(__dirname + "/public"));

// ===== ESP32 Status =====
let lastSeen = null;
mqttClient.subscribe("home/sensor/data");
mqttClient.on("message", (topic) => {
  if (topic === "home/sensor/data") lastSeen = Date.now();
});
app.get("/api/esp32/status", (req, res) => {
  if (!lastSeen) return res.json({ connected: false });
  const secondsSinceLast = (Date.now() - lastSeen) / 1000;
  res.json({ connected: secondsSinceLast < 15 });
});

// ===== Start Server =====
const port = PORT || 8080;
app.listen(port, "0.0.0.0", () => {
  console.log(`✅ Server đang chạy tại: http://localhost:${port}`);
});