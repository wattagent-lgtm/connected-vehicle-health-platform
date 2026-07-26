# Vehicle Health Dashboard and API

AWS SAM application for the `CAR-01-OBD` dashboard backend.

It queries the existing `iiot-gateway-telemetry` DynamoDB table, merges the
newest fast and slow OBD data, and returns a demonstration health score.
The same stack hosts the static cockpit dashboard in a private S3 bucket
behind an HTTPS CloudFront distribution. The current dashboard includes live
OBD parameters, DTC visibility, health/predictive-maintenance indicators, and
trip/fuel estimation with confidence and coverage.

## Prerequisites

- AWS CLI v2
- AWS SAM CLI
- An AWS identity permitted to deploy CloudFormation, Lambda, API Gateway,
  IAM roles, and read the existing DynamoDB table

## Deploy

```powershell
aws configure
aws sts get-caller-identity

cd connected-vehicle-health-platform\aws-dashboard
.\deploy.ps1
```

For a named AWS CLI profile:

```powershell
.\deploy.ps1 -AwsProfile my-profile
```

## Test

Copy `ApiBaseUrl` from the deployment output:

```powershell
.\test_api.ps1 -ApiBaseUrl "https://API_ID.execute-api.ap-southeast-1.amazonaws.com/v1"
```

## Routes

- `GET /api/cars/{device_id}/health`
- `GET /api/cars/{device_id}/latest`
- `GET /api/cars/{device_id}/history?minutes=60`
- `GET /api/cars/{device_id}/trip`

The scoring thresholds and trip/fuel calculations are for laboratory
demonstration only. They are not manufacturer limits, a billing-grade fuel
meter, or a safety diagnostic.
