#pragma once
static bool wdtApply(uint32_t ms)
{
  const esp_task_wdt_config_t cfg = {
      .timeout_ms = ms,
      .idle_core_mask = 0,
      .trigger_panic = true};
  esp_err_t e = esp_task_wdt_reconfigure(&cfg);
  if (e != ESP_OK)
    LOG_E("[WDT] reconfigure %lu ms gagal err=%d\n", (unsigned long)ms, (int)e);
  else
    LOG_I("[WDT] timeout %lu ms\n", (unsigned long)ms);
  return e == ESP_OK;
}

void extendWdtForSync()
{
  portENTER_CRITICAL(&wdtMux);
  bool already = wdtExtended;
  wdtExtended = true;
  portEXIT_CRITICAL(&wdtMux);
  if (already)
    return;
  if (!wdtApply(WDT_SYNC_TIMEOUT_MS))
    wdtExtended = false;
}

void restoreWdtNormal()
{
  portENTER_CRITICAL(&wdtMux);
  bool was = wdtExtended;
  wdtExtended = false;
  portEXIT_CRITICAL(&wdtMux);
  if (!was)
    return;
  if (!wdtApply(WDT_NORMAL_TIMEOUT_MS))
    wdtExtended = true;
}
