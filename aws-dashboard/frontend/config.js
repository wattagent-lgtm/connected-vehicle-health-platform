window.CAR_HEALTH_CONFIG = {
  // Empty means the dashboard calls the API on the same origin. Set an
  // explicit API Gateway stage URL only when hosting the frontend separately.
  apiBaseUrl: "",
  deviceId: "CAR-01-OBD",
  refreshMilliseconds: 1000,
  historyRefreshMilliseconds: 30000
};
