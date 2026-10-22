#pragma once
static uint8_t failOta = 0;

static uint8_t failDb = 0;

static uint8_t failTelemetry = 0;

static uint8_t failConfig = 0;

static void backoffFail(uint8_t &f)
{
  if (f < 255)
    f++;
}

static void resetPollBackoff()
{
  failOta = 0;
  failDb = 0;
  failTelemetry = 0;
  failConfig = 0;
}
