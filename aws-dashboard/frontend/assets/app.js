(() => {
  "use strict";

  const config = window.CAR_HEALTH_CONFIG;
  const $ = (id) => document.getElementById(id);
  const healthUrl = `${config.apiBaseUrl}/api/cars/${encodeURIComponent(config.deviceId)}/health`;
  const historyUrl = `${config.apiBaseUrl}/api/cars/${encodeURIComponent(config.deviceId)}/history?minutes=60`;
  const maintenanceUrl = `${config.apiBaseUrl}/api/cars/${encodeURIComponent(config.deviceId)}/maintenance`;
  const tripUrl = `${config.apiBaseUrl}/api/cars/${encodeURIComponent(config.deviceId)}/trip`;
  let chartSeries = [];
  let maintenanceLoaded = false;
  const liveSeries = [];
  const enabledSignals = new Set([
    "engine_rpm", "vehicle_speed_kph", "coolant_c", "engine_load_pct", "throttle_pct"
  ]);

  const number = (value, digits = 0) => {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed.toFixed(digits) : "--";
  };

  const setConnection = (state, label) => {
    $("connection-dot").className = `dot ${state}`;
    $("connection-label").textContent = label;
  };

  const setMetric = (id, value, digits = 0) => {
    $(id).textContent = number(value, digits);
  };

  const setPidMetric = (id, value, digits = 0) => {
    const parsed = Number(value);
    $(id).textContent = Number.isFinite(parsed) ? parsed.toFixed(digits) : "N/S";
    $(id).title = Number.isFinite(parsed)
      ? ""
      : "Not supplied by this vehicle ECU using the standard OBD-II PID";
  };

  const valueWithUnit = (value, unit, digits = 1) => {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? `${parsed.toFixed(digits)} ${unit}` : "--";
  };

  const fuelSystemName = (value) => ({
    1: "OPEN LOOP", 2: "CLOSED LOOP", 4: "OPEN / LOAD",
    8: "OPEN / FAULT", 16: "CLOSED / FAULT"
  })[Number(value)] || (Number.isFinite(Number(value)) ? `CODE ${value}` : "--");

  const obdStandardName = (value) => ({
    1: "OBD-II (CARB)", 2: "OBD (EPA)", 3: "OBD + OBD-II",
    4: "OBD-I", 5: "Not OBD", 6: "EOBD", 7: "EOBD + OBD-II",
    11: "JOBD", 13: "JOBD + EOBD + OBD-II", 17: "EMD",
    21: "WWH-OBD", 23: "HD EOBD-I", 24: "HD EOBD-I N"
  })[Number(value)] || (Number.isFinite(Number(value)) ? `Standard ${value}` : "--");

  const fuelTypeName = (value) => ({
    1: "Gasoline", 2: "Methanol", 3: "Ethanol", 4: "Diesel",
    5: "LPG", 6: "CNG", 8: "Electric", 9: "Bifuel gasoline",
    17: "Bifuel gasoline/CNG", 19: "Bifuel gasoline/LPG",
    23: "Bifuel diesel", 24: "Bifuel diesel/CNG"
  })[Number(value)] || (Number.isFinite(Number(value)) ? `Type ${value}` : "--");

  const setBar = (id, valueId, value, maximum, unit, digits = 0) => {
    const parsed = Number(value);
    const percentage = Number.isFinite(parsed)
      ? Math.max(0, Math.min(100, parsed / maximum * 100))
      : 0;
    $(id).style.width = `${percentage}%`;
    $(id).classList.toggle("unknown", !Number.isFinite(parsed));
    $(valueId).textContent = Number.isFinite(parsed)
      ? `${parsed.toFixed(digits)} ${unit}`
      : "--";
  };

  const setDial = (needleId, valueId, value, maximum) => {
    const parsed = Number(value);
    const ratio = Number.isFinite(parsed) ? Math.max(0, Math.min(1, parsed / maximum)) : 0;
    $(needleId).style.transform = `rotate(${-125 + ratio * 250}deg)`;
    $(valueId).textContent = Number.isFinite(parsed) ? parsed.toFixed(0) : "--";
  };

  const formatTime = (milliseconds) => {
    const value = Number(milliseconds);
    return Number.isFinite(value)
      ? new Date(value).toLocaleString()
      : "Unknown";
  };

  const duration = (seconds) => {
    const value = Number(seconds);
    if (!Number.isFinite(value)) return "--";
    const hours = Math.floor(value / 3600);
    const minutes = Math.floor((value % 3600) / 60);
    return `${hours}h ${minutes}m`;
  };

  const renderDtc = (id, values) => {
    const codes = Array.isArray(values)
      ? values.map((code) => String(code).trim().toUpperCase()).filter((code) =>
          /^[PBCU][0-3][0-9A-F]{3}$/.test(code) &&
          code !== "P0000" && code !== "P00FF" && !code.endsWith("FFF"))
      : [];
    $(id).innerHTML = codes.length
      ? codes.map((code) => `<span class="dtc-code">${escapeHtml(code)}</span>`).join("")
      : "<span>None</span>";
    return codes.length;
  };

  const renderHealth = (payload) => {
    const telemetry = payload.telemetry || {};
    const warnings = Array.isArray(payload.warnings) ? payload.warnings : [];

    $("vehicle-name").textContent = payload.device_name || payload.device_id;
    $("device-id").textContent = payload.device_id;
    $("source-mode").textContent = String(payload.source_mode || "unknown").toUpperCase();
    $("last-update").textContent = `Updated ${formatTime(payload.received_at)}`;
    $("health-score").textContent = number(payload.health_score);
    $("health-level").textContent = payload.health_level || "UNKNOWN";
    $("health-level").className = `health-${String(payload.health_level || "unknown").toLowerCase()}`;
    $("health-summary").textContent = warnings.length
      ? `${warnings.length} condition${warnings.length > 1 ? "s" : ""} require attention`
      : "All monitored conditions are within limits";

    setMetric("engine-rpm", telemetry.engine_rpm);
    setMetric("vehicle-speed", telemetry.vehicle_speed_kph, 1);
    setMetric("coolant", telemetry.coolant_c, 1);
    setPidMetric("fuel-level", telemetry.fuel_level_pct, 1);
    setMetric("module-voltage", telemetry.control_module_voltage_v, 2);
    setPidMetric("ambient-air", telemetry.ambient_air_c, 1);
    $("engine-runtime").textContent = duration(telemetry.engine_runtime_s);
    $("vehicle-link").textContent =
      payload.vehicle_link_state || telemetry.vehicle_link_state || payload.status || "UNKNOWN";
    $("engine-load").textContent = `${number(telemetry.engine_load_pct, 1)} %`;
    $("throttle").textContent = `${number(telemetry.throttle_pct, 1)} %`;
    $("intake-air").textContent = `${number(telemetry.intake_air_c, 1)} °C`;
    $("obd-success").textContent = number(telemetry.obd_success_count);
    $("obd-errors").textContent = number(telemetry.obd_error_count);
    $("obd-data-age").textContent = Number(telemetry.obd_data_age_ms) >= 0
      ? `${number(Number(telemetry.obd_data_age_ms) / 1000, 1)} sec`
      : "--";
    $("data-age").textContent = `${number(payload.last_seen_seconds)} sec`;
    $("pid-mask").textContent = telemetry.supported_pid_mask ?? "--";
    $("client-heap").textContent = telemetry.free_heap_bytes
      ? `${Math.round(telemetry.free_heap_bytes / 1024)} KB`
      : "--";
    $("data-classes").textContent = (payload.available_data_classes || []).join(", ") || "--";
    $("read-mode").textContent = telemetry.read_only === true ? "READ ONLY" : "UNKNOWN";
    $("records-evaluated").textContent = `${payload.records_evaluated || 0} records evaluated`;

    $("fuel-system-status").textContent = fuelSystemName(telemetry.fuel_system_status);
    $("short-fuel-trim").textContent = valueWithUnit(telemetry.short_fuel_trim_b1_pct, "%");
    $("long-fuel-trim").textContent = valueWithUnit(telemetry.long_fuel_trim_b1_pct, "%");
    $("intake-manifold").textContent = valueWithUnit(telemetry.intake_manifold_kpa, "kPa");
    $("mass-airflow").textContent = valueWithUnit(telemetry.maf_g_s, "g/s", 2);
    $("engine-fuel-rate").textContent = valueWithUnit(telemetry.engine_fuel_rate_l_h, "L/h", 2);
    const distanceMil = Number(telemetry.distance_mil_km);
    const distanceClear = Number(telemetry.distance_since_clear_km);
    $("distance-mil").textContent = Number.isFinite(distanceMil) && distanceMil < 65535
      ? `${distanceMil.toFixed(0)} km` : "N/S";
    $("distance-clear").textContent = Number.isFinite(distanceClear) && distanceClear < 65535
      ? `${distanceClear.toFixed(0)} km` : "N/S";
    $("obd-standard").textContent = obdStandardName(telemetry.obd_standard);
    $("fuel-type").textContent = fuelTypeName(telemetry.fuel_type);
    const extendedKeys = [
      "fuel_system_status", "short_fuel_trim_b1_pct", "long_fuel_trim_b1_pct",
      "intake_manifold_kpa", "maf_g_s", "engine_fuel_rate_l_h",
      "distance_mil_km", "distance_since_clear_km", "obd_standard", "fuel_type"
    ];
    const supportedExtended = extendedKeys.filter((key) => {
      const value = telemetry[key];
      return value !== undefined && value !== null && value !== "" &&
        (typeof value !== "number" || Number.isFinite(value));
    }).length;
    $("extended-support").textContent = `${supportedExtended}/${extendedKeys.length} supported`;

    setBar("bar-rpm", "bar-rpm-value", telemetry.engine_rpm, 7000, "rpm");
    setBar("bar-speed", "bar-speed-value", telemetry.vehicle_speed_kph, 200, "km/h", 1);
    setBar("bar-load", "bar-load-value", telemetry.engine_load_pct, 100, "%", 1);
    setBar("bar-throttle", "bar-throttle-value", telemetry.throttle_pct, 100, "%", 1);
    setBar("bar-coolant", "bar-coolant-value", telemetry.coolant_c, 120, "°C", 1);
    setBar("bar-fuel", "bar-fuel-value", telemetry.fuel_level_pct, 100, "%", 1);
    setDial("rpm-needle", "dial-rpm", telemetry.engine_rpm, 7000);
    setDial("speed-needle", "dial-speed", telemetry.vehicle_speed_kph, 200);
    const speed = Number(telemetry.vehicle_speed_kph);
    const rpm = Number(telemetry.engine_rpm);
    $("drive-state").textContent = Number.isFinite(speed) && speed > 1
      ? "DRIVING" : Number.isFinite(rpm) && rpm > 300 ? "IDLING" : "STANDBY";
    $("drive-detail").textContent = `${number(telemetry.engine_load_pct, 1)}% load · ${number(telemetry.throttle_pct, 1)}% throttle`;
    $("chip-link").className = String(payload.vehicle_link_state || "").toUpperCase() === "ONLINE" ? "ok" : "warn";
    const voltage = Number(telemetry.control_module_voltage_v);
    $("chip-voltage").className = Number.isFinite(voltage) && voltage >= 11.8 && voltage <= 15.2 ? "ok" : "warn";

    $("warning-count").textContent = warnings.length;
    $("warnings").innerHTML = warnings.length
      ? warnings.map((item) => `<div class="warning">${escapeHtml(item)}</div>`).join("")
      : '<p class="empty">No active vehicle warnings.</p>';

    const dtcCount =
      renderDtc("dtc-stored", telemetry.stored_dtc) +
      renderDtc("dtc-pending", telemetry.pending_dtc) +
      renderDtc("dtc-permanent", telemetry.permanent_dtc);
    $("dtc-count").textContent = dtcCount;
    $("chip-dtc").className = dtcCount ? "alert" : "ok";

    const linkOnline = String(payload.vehicle_link_state || telemetry.vehicle_link_state || "").toUpperCase() === "ONLINE";
    const dataFresh = Number(payload.last_seen_seconds) <= 10;
    const setLamp = (id, state, text) => {
      $(id).className = `ann-lamp ${state}`;
      $(id).querySelector("strong").textContent = text;
    };
    setLamp("ann-vehicle", payload.status === "ONLINE" ? "ok" : "alert", payload.status || "UNKNOWN");
    setLamp("ann-obd", linkOnline ? "ok" : "warn", linkOnline ? "LINKED" : "NO LINK");
    setLamp("ann-battery", Number.isFinite(voltage) && voltage >= 11.8 && voltage <= 15.2 ? "ok" : "warn",
      Number.isFinite(voltage) ? `${voltage.toFixed(1)} V` : "NO DATA");
    setLamp("ann-dtc", dtcCount ? "alert" : "ok", dtcCount ? `${dtcCount} ACTIVE` : "CLEAR");
    setLamp("ann-cloud", "ok", "CONNECTED");
    setLamp("ann-data", dataFresh ? "ok" : "warn", `${number(payload.last_seen_seconds)} SEC`);

    const batteryHealthy = Number.isFinite(voltage) && voltage >= 11.8 && voltage <= 15.2;
    $("maintenance-battery").textContent = Number.isFinite(voltage)
      ? batteryHealthy ? `Normal · ${voltage.toFixed(1)} V` : `Check · ${voltage.toFixed(1)} V`
      : "No voltage data";
    $("maintenance-battery").className = batteryHealthy ? "service-ok" : "service-warn";
    $("maintenance-dtc").textContent = dtcCount
      ? `${dtcCount} code${dtcCount > 1 ? "s" : ""} need review`
      : "No active codes";
    $("maintenance-dtc").className = dtcCount ? "service-warn" : "service-ok";

    liveSeries.push({
      timestamp: Date.now(),
      engine_rpm: telemetry.engine_rpm,
      vehicle_speed_kph: telemetry.vehicle_speed_kph,
      coolant_c: telemetry.coolant_c,
      engine_load_pct: telemetry.engine_load_pct,
      throttle_pct: telemetry.throttle_pct
    });
    while (liveSeries.length > 180) liveSeries.shift();
    drawChart();

    const online = payload.status === "ONLINE";
    setConnection(online ? "online" : "offline", payload.status || "UNKNOWN");
  };

  const escapeHtml = (value) => String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");

  const parseRecordData = (value) => {
    if (value && typeof value === "object") return value;
    if (typeof value === "string") {
      try { return JSON.parse(value); } catch (_) { return {}; }
    }
    return {};
  };

  const buildSeries = (records) => {
    const merged = new Map();
    [...records].reverse().forEach((record) => {
      const timestamp = Number(record.received_at);
      const bucket = Math.floor(timestamp / 5000) * 5000;
      const current = merged.get(bucket) || { timestamp: bucket };
      Object.assign(current, parseRecordData(record.data));
      merged.set(bucket, current);
    });
    return [...merged.values()].slice(-60);
  };

  const drawChart = () => {
    const canvas = $("history-chart");
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.floor(rect.width * dpr));
    canvas.height = Math.max(1, Math.floor(rect.height * dpr));
    const ctx = canvas.getContext("2d");
    ctx.scale(dpr, dpr);
    const width = rect.width;
    const height = rect.height;
    const pad = { left: 38, right: 12, top: 12, bottom: 24 };
    const plotWidth = width - pad.left - pad.right;
    const plotHeight = height - pad.top - pad.bottom;

    ctx.clearRect(0, 0, width, height);
    ctx.strokeStyle = "rgba(83,116,129,.25)";
    ctx.fillStyle = "#66838f";
    ctx.font = "10px Segoe UI";
    ctx.lineWidth = 1;

    for (let i = 0; i <= 4; i += 1) {
      const y = pad.top + (plotHeight * i / 4);
      ctx.beginPath();
      ctx.moveTo(pad.left, y);
      ctx.lineTo(width - pad.right, y);
      ctx.stroke();
      ctx.fillText(`${100 - i * 25}%`, 2, y + 3);
    }

    const sourceSeries = liveSeries.length > 1 ? liveSeries : chartSeries;
    if (sourceSeries.length < 2) {
      ctx.fillText("Collecting historical telemetry…", pad.left + 12, height / 2);
      return;
    }

    const specifications = [
      { key: "engine_rpm", color: "#1de9c5" },
      { key: "vehicle_speed_kph", color: "#4e9cff" },
      { key: "coolant_c", color: "#ffad42" },
      { key: "engine_load_pct", color: "#ff6178" },
      { key: "throttle_pct", color: "#b889ff" }
    ];

    specifications.filter(({ key }) => enabledSignals.has(key)).forEach(({ key, color }) => {
      const values = sourceSeries.map((item) => Number(item[key])).filter(Number.isFinite);
      if (values.length < 2) return;
      const min = Math.min(...values);
      const max = Math.max(...values);
      const range = Math.max(1, max - min);
      let started = false;
      ctx.beginPath();
      sourceSeries.forEach((item, index) => {
        const value = Number(item[key]);
        if (!Number.isFinite(value)) return;
        const x = pad.left + plotWidth * index / Math.max(1, sourceSeries.length - 1);
        const y = pad.top + plotHeight * (1 - (value - min) / range);
        if (!started) { ctx.moveTo(x, y); started = true; } else { ctx.lineTo(x, y); }
      });
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.stroke();
    });

    const first = new Date(sourceSeries[0].timestamp);
    const last = new Date(sourceSeries.at(-1).timestamp);
    ctx.fillStyle = "#66838f";
    ctx.fillText(first.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }), pad.left, height - 5);
    const endLabel = last.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    ctx.fillText(endLabel, width - pad.right - ctx.measureText(endLabel).width, height - 5);
  };

  const loadHealth = async () => {
    try {
      const response = await fetch(healthUrl, { cache: "no-store" });
      if (!response.ok) throw new Error(`Health API returned HTTP ${response.status}`);
      renderHealth(await response.json());
    } catch (error) {
      setConnection("offline", "API ERROR");
      $("health-summary").textContent = error.message;
      $("health-summary").classList.add("error-text");
    }
  };

  const loadHistory = async () => {
    try {
      const response = await fetch(historyUrl, { cache: "no-store" });
      if (!response.ok) throw new Error(`History API returned HTTP ${response.status}`);
      const payload = await response.json();
      chartSeries = buildSeries(payload.records || []);
      drawChart();
    } catch (error) {
      console.error(error);
    }
  };

  const renderMaintenance = (payload) => {
    const prediction = payload.predictive_maintenance || {};
    const findings = Array.isArray(prediction.findings) ? prediction.findings : [];
    const status = String(prediction.overall_status || "INSUFFICIENT_DATA").toUpperCase();
    const statusLabel = status.replaceAll("_", " ");
    $("prediction-status").textContent = statusLabel;
    $("prediction-status").className = `prediction-state ${status.toLowerCase()}`;
    $("prediction-headline").textContent = prediction.actionable_systems
      ? `${prediction.actionable_systems} system${prediction.actionable_systems > 1 ? "s" : ""} need attention`
      : "No developing condition detected in the available evidence";
    $("prediction-window").textContent =
      `${number(prediction.records_evaluated)} records · ${number(prediction.observed_minutes)} min observation window`;

    const baseline = prediction.baseline || {};
    $("baseline-status").textContent = baseline.status || "COLLECTING";
    $("baseline-message").textContent = baseline.message || "Continue collecting comparable trips.";

    const container = $("prediction-findings");
    container.innerHTML = "";
    findings.forEach((finding) => {
      const card = document.createElement("article");
      const findingStatus = String(finding.status || "INSUFFICIENT_DATA").toLowerCase();
      card.className = `prediction-card ${findingStatus}`;
      card.innerHTML = `
        <div class="prediction-card-head">
          <strong>${finding.label || finding.system}</strong>
          <span>${String(finding.status || "INSUFFICIENT_DATA").replaceAll("_", " ")}</span>
        </div>
        <p>${finding.evidence || "No evidence available."}</p>
        <div class="prediction-action"><small>RECOMMENDED ACTION</small>${finding.recommendation || "Continue monitoring."}</div>
        <footer><span>${number(finding.sample_count)} samples</span><span>${finding.confidence || "LOW"} confidence</span></footer>`;
      container.appendChild(card);
    });
  };

  const loadMaintenance = async () => {
    try {
      let response = await fetch(maintenanceUrl, { cache: "no-store" });
      if (response.status === 503) {
        await new Promise((resolve) => setTimeout(resolve, 1200));
        response = await fetch(maintenanceUrl, { cache: "no-store" });
      }
      if (!response.ok) throw new Error(`Maintenance API returned HTTP ${response.status}`);
      renderMaintenance(await response.json());
      maintenanceLoaded = true;
    } catch (error) {
      // A cloud/API delay is not a vehicle fault. Keep the last good analysis visible.
      $("prediction-status").textContent = "DATA DELAY";
      $("prediction-status").className = "prediction-state insufficient_data";
      $("prediction-window").textContent = maintenanceLoaded
        ? `Last successful analysis retained · ${error.message} · automatic retry in 60 sec`
        : `${error.message} · automatic retry in 60 sec`;
      if (!maintenanceLoaded) {
        $("prediction-headline").textContent =
          "Predictive analysis is temporarily unavailable; live vehicle telemetry remains active";
      }
    }
  };

  const renderTrip = (payload) => {
    const trip = payload.trip_estimate || {};
    const display = (value, digits = 2) => {
      const parsed = Number(value);
      return Number.isFinite(parsed) ? parsed.toFixed(digits) : "--";
    };
    $("trip-status").textContent = trip.status || "UNKNOWN";
    $("trip-confidence").textContent = `${trip.confidence || "LOW"} CONFIDENCE`;
    $("trip-confidence").className = `confidence-badge ${String(trip.confidence || "low").toLowerCase()}`;
    $("trip-distance").textContent = display(trip.distance_km, 2);
    $("trip-fuel-used").textContent = display(trip.fuel_used_l, 3);
    $("trip-economy").textContent = display(trip.average_l_per_100km, 2);
    $("trip-km-l").textContent = display(trip.average_km_per_l, 2);
    $("trip-duration").textContent = duration(trip.duration_seconds);
    $("trip-average-speed").textContent = `${display(trip.average_speed_kph, 1)} km/h`;
    $("trip-max-speed").textContent = `${display(trip.max_speed_kph, 1)} km/h`;
    $("trip-max-rpm").textContent = `${display(trip.max_engine_rpm, 0)} rpm`;
    $("trip-fuel-rate").textContent = `${display(trip.instant_fuel_rate_l_h, 2)} L/h`;
    $("trip-instant-economy").textContent = Number.isFinite(Number(trip.instant_l_per_100km))
      ? `${display(trip.instant_l_per_100km, 2)} L/100 km`
      : "N/A below 5 km/h";
    $("trip-idle-time").textContent = duration(trip.idle_seconds);
    $("trip-idle-fuel").textContent = `${display(trip.idle_fuel_l, 3)} L`;
    const coverage = Math.max(0, Math.min(100, Number(trip.data_coverage_pct) || 0));
    $("trip-coverage").textContent = `${coverage.toFixed(1)}%`;
    $("trip-coverage-bar").style.width = `${coverage}%`;
    $("trip-method").textContent = trip.method === "PID_5E_DIRECT"
      ? "Fuel source: direct OBD PID 5E"
      : trip.method === "MAF_GASOLINE_ESTIMATE"
        ? "Fuel source: MAF gasoline estimate"
        : "Fuel source unavailable";
    $("trip-window").textContent = trip.trip_start
      ? `${formatTime(trip.trip_start)} to ${formatTime(trip.trip_end)} · ${trip.valid_intervals || 0} valid intervals · ${trip.gap_count || 0} gaps`
      : "No valid engine-running trip window";
  };

  const loadTrip = async () => {
    try {
      const response = await fetch(tripUrl, { cache: "no-store" });
      if (!response.ok) throw new Error(`Trip API returned HTTP ${response.status}`);
      renderTrip(await response.json());
    } catch (error) {
      $("trip-status").textContent = "DATA DELAY";
      $("trip-method").textContent = `${error.message} · automatic retry`;
    }
  };

  window.addEventListener("resize", drawChart);
  document.querySelectorAll(".signal-toggle").forEach((button) => {
    button.addEventListener("click", () => {
      const signal = button.dataset.signal;
      if (enabledSignals.has(signal)) enabledSignals.delete(signal); else enabledSignals.add(signal);
      button.classList.toggle("active", enabledSignals.has(signal));
      drawChart();
    });
  });
  loadHealth();
  loadHistory();
  loadMaintenance();
  loadTrip();
  setInterval(loadHealth, config.refreshMilliseconds);
  setInterval(loadHistory, config.historyRefreshMilliseconds);
  setInterval(loadMaintenance, 60000);
  setInterval(loadTrip, 30000);
})();
