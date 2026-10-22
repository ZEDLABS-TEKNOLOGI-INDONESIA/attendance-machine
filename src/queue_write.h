#pragma once
bool initSDCard()
{
  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_RFID_SS, OUTPUT);
  deselectSD();
  digitalWrite(PIN_RFID_SS, HIGH);
  selectSD();
  delay(10);
  if (!sd.begin(PIN_SD_CS, SD_SCK_MHZ(4)))
  {
    sdInitErr = sd.sdErrorCode();
    deselectSD();
    return false;
  }
  loadMetadataLocked();
  recoverKeepTmpLocked();
  pendingCacheDirty = true;
  if (!rtcQueueFileValid)
  {
    currentQueueFile = -1;
    char fn[20];
    for (int i = 0; i < MAX_QUEUE_FILES; i++)
    {
      esp_task_wdt_reset();
      getQueueFileName(i, fn, sizeof(fn));
      if (!sd.exists(fn))
      {
        if (file.open(fn, O_WRONLY | O_CREAT))
        {
          file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
          file.close();
          currentQueueFile = i;
          break;
        }
      }
      else
      {
        int cnt = countValidRecordsInFileLocked(fn);
        if (cnt == 0)
        {
          if (removeQueueFileSafeLocked(fn, 0) && file.open(fn, O_WRONLY | O_CREAT))
          {
            file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
            file.close();
            currentQueueFile = i;
            break;
          }
        }
        else if (cnt < MAX_RECORDS_PER_FILE)
        {
          currentQueueFile = i;
          break;
        }
      }
    }
    if (currentQueueFile == -1)
    {
      currentQueueFile = 0;
    }
    rtcQueueFileValid = true;
  }
  deselectSD();
  return true;
}

bool findAvailableQueueSlotLocked(int startIdx, int *outIdx)
{
  char fn[20];
  for (int offset = 0; offset < QUEUE_SLOT_SEARCH_LIMIT; offset++)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int idx = (startIdx + offset) % MAX_QUEUE_FILES;
    if (idx == syncingQueueFile)
      continue;
    getQueueFileName(idx, fn, sizeof(fn));
    if (!sd.exists(fn))
    {
      *outIdx = idx;
      return true;
    }
    int cnt = countValidRecordsInFileLocked(fn);
    if (cnt == 0)
    {
      if (removeQueueFileSafeLocked(fn, 0))
      {
        *outIdx = idx;
        return true;
      }
      continue;
    }
    if (cnt < MAX_RECORDS_PER_FILE)
    {
      *outIdx = idx;
      return true;
    }
  }
  return false;
}

SaveResult saveToQueue(const char *rfid, const char *ts, unsigned long t)
{
  if (!sdCardAvailable)
  {
    return SAVE_SD_ERROR;
  }
  if (!acquireSD())
  {
    return SAVE_SD_ERROR;
  }
  selectSD();
  bool dup = isDuplicateLocked(rfid, t);
  if (dup)
  {
    deselectSD();
    releaseSD();
    return SAVE_DUPLICATE;
  }
  bool fileSwitched = false;
  if (currentQueueFile < 0 || currentQueueFile >= MAX_QUEUE_FILES)
  {
    currentQueueFile = 0;
    fileSwitched = true;
  }
  char curFn[20];
  getQueueFileName(currentQueueFile, curFn, sizeof(curFn));
  if (!sd.exists(curFn))
  {
    if (file.open(curFn, O_WRONLY | O_CREAT))
    {
      file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
      file.close();
    }
  }
  int curCnt = countRecordsInFileLocked(curFn);
  if (curCnt >= MAX_RECORDS_PER_FILE)
  {
    int nextIdx;
    int startSearch = (currentQueueFile + 1) % MAX_QUEUE_FILES;
    if (!findAvailableQueueSlotLocked(startSearch, &nextIdx))
    {
      deselectSD();
      releaseSD();
      return SAVE_QUEUE_FULL;
    }
    currentQueueFile = nextIdx;
    fileSwitched = true;
    getQueueFileName(currentQueueFile, curFn, sizeof(curFn));
    if (!file.open(curFn, O_WRONLY | O_CREAT))
    {
      deselectSD();
      releaseSD();
      return SAVE_SD_ERROR;
    }
    file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
    file.close();
  }
  if (!file.open(curFn, O_RDWR | O_APPEND))
  {
    deselectSD();
    releaseSD();
    return SAVE_SD_ERROR;
  }

  if (!prepareAppendLocked(file, QUEUE_HEADER_LINE))
  {
    file.close();
    deselectSD();
    releaseSD();
    return SAVE_SD_ERROR;
  }
  uint8_t crc = recordCrc8(rfid, t);

  char lineBuf[96];
  int n = snprintf(lineBuf, sizeof(lineBuf), "%s,%s,%s,%lu,%02X\r\n", rfid, ts, deviceId, t, (unsigned)crc);
  if (n <= 0 || n >= (int)sizeof(lineBuf) || file.write(lineBuf, (size_t)n) != (size_t)n)
  {
    file.close();
    deselectSD();
    releaseSD();
    return SAVE_SD_ERROR;
  }
  file.sync();
  file.close();
  deselectSD();
  cachedPendingRecords++;
  if (cachedPendingRecords < 0)
    cachedPendingRecords = 0;
  queueMutations = queueMutations + 1;
  if (fileSwitched)
    saveMetadataLocked();
  releaseSD();
  return SAVE_OK;
}
