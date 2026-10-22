#pragma once
static bool prepareAppendLocked(FsFile &f, const char *header)
{
  uint64_t sz = f.size();
  if (sz == 0)
  {
    if (header && f.println(header) == 0)
      return false;
    return true;
  }
  if (!f.seekSet(sz - 1))
    return false;
  int last = f.read();
  if (last < 0)
    return false;
  if (last != '\n')
  {
    if (f.write((uint8_t)'\n') != 1)
      return false;
  }
  return true;
}

void appendFailedLogToSD(const char *rfid, const char *ts, const char *reason)
{
  if (!sdCardAvailable)
    return;
  if (!acquireSD(pdMS_TO_TICKS(2000)))
    return;
  selectSD();

  int lineCount = 0;
  if (sd.exists("/failed_log.csv"))
  {
    FsFile countFile;
    if (countFile.open("/failed_log.csv", O_RDONLY))
    {
      char ln[128];
      while (countFile.fgets(ln, sizeof(ln)) > 0)
      {
        size_t l = strlen(ln);
        if (l > 0 && ln[l - 1] == '\n')
          lineCount++;
      }
      countFile.close();
    }
  }

  if (lineCount >= FAILED_LOG_MAX_LINES)
  {
    if (sd.exists("/failed_log.old"))
      sd.remove("/failed_log.old");
    sd.rename("/failed_log.csv", "/failed_log.old");
    lineCount = 0;
  }
  if (lineCount < FAILED_LOG_MAX_LINES)
  {
    FsFile logFile;
    if (logFile.open("/failed_log.csv", O_RDWR | O_CREAT | O_APPEND))
    {
      if (prepareAppendLocked(logFile, "rfid,timestamp,reason"))
      {
        logFile.print(rfid);
        logFile.print(',');
        logFile.print(ts);
        logFile.print(',');
        logFile.println(reason);
        logFile.sync();
      }
      logFile.close();
    }
    else
    {
      showOLED(F("ERROR"), "GAGAL MENYIMPAN LOG");
    }
  }

  deselectSD();
  releaseSD();
}
