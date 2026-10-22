#pragma once
bool fileHasValidRecords(const char *fn)
{
  if (!file.open(fn, O_RDONLY))
    return false;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  char line[144];
  if (file.available())
    file.fgets(line, sizeof(line));
  bool found = false;
  while (file.available())
  {
    esp_task_wdt_reset();
    if (file.fgets(line, sizeof(line)) <= 0)
      break;
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    char *c1 = strchr(line, ',');
    char *c2 = c1 ? strchr(c1 + 1, ',') : nullptr;
    char *c3 = c2 ? strchr(c2 + 1, ',') : nullptr;
    if (!c3)
      continue;
    unsigned long recT = strtoul(c3 + 1, nullptr, 10);
    if (recT > 0 && !isRecordExpired(recT, now, nowValid))
    {
      found = true;
      break;
    }
  }
  file.close();
  return found;
}

static int classifyQueueLine(char *line, OfflineRecord *out, time_t now, bool nowValid);

int countValidRecordsInFileLocked(const char *fn)
{
  if (!file.open(fn, O_RDONLY))
    return 0;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  int cnt = 0;
  char line[144];
  if (file.available())
    file.fgets(line, sizeof(line));
  while (file.available())
  {
    esp_task_wdt_reset();
    taskYIELD();
    if (file.fgets(line, sizeof(line)) <= 0)
      break;
    if (classifyQueueLine(line, nullptr, now, nowValid) == LINE_OK)
      cnt++;
  }
  file.close();
  return cnt;
}

int countAllOfflineRecords()
{
  if (!sdCardAvailable)
    return 0;
  int total = 0;
  int files = 0;
  int from = 0;
  char fn[24];
  for (;;)
  {
    esp_task_wdt_reset();
    vTaskDelay(1);
    if (!acquireSD(pdMS_TO_TICKS(2000)))
      return -1;
    selectSD();
    int idx;
    if (!findNextQueueFileLocked(from, &idx))
    {
      deselectSD();
      releaseSD();
      break;
    }
    getQueueFileName(idx, fn, sizeof(fn));
    int v = countValidRecordsInFileLocked(fn);
    deselectSD();
    releaseSD();
    if (v > 0)
    {
      files++;
      total += v;
    }
    from = idx + 1;
  }
  cachedQueueFileCount = files;
  return total;
}

void refreshPendingCache()
{
  if (!pendingCacheDirty)
    return;
  pendingCacheDirty = false;
  uint32_t before = queueMutations;
  int n = countAllOfflineRecords();
  if (n < 0 || queueMutations != before)
  {
    pendingCacheDirty = true;
    return;
  }
  cachedPendingRecords = n;
  if (!acquireSD(pdMS_TO_TICKS(1000)))
    return;
  saveMetadataLocked();
  releaseSD();
}

bool isDuplicateLocked(const char *rfid, unsigned long t)
{
  char fn[20];
  for (int offset = 0; offset < MAX_DUPLICATE_CHECK_FILES; offset++)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int idx = (currentQueueFile - offset + MAX_QUEUE_FILES) % MAX_QUEUE_FILES;
    getQueueFileName(idx, fn, sizeof(fn));
    if (!sd.exists(fn))
      continue;
    if (!file.open(fn, O_RDONLY))
      continue;
    char line[128];
    if (file.available())
      file.fgets(line, sizeof(line));
    int read = 0;
    bool found = false;
    while (read < MAX_DUPLICATE_CHECK_LINES && file.fgets(line, sizeof(line)) > 0)
    {
      esp_task_wdt_reset();
      int len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      char tmp[128];
      strncpy(tmp, line, sizeof(tmp) - 1);
      tmp[sizeof(tmp) - 1] = '\0';
      char *c1 = strchr(tmp, ',');
      if (!c1)
      {
        read++;
        continue;
      }
      char *c2 = strchr(c1 + 1, ',');
      if (!c2)
      {
        read++;
        continue;
      }
      char *c3 = strchr(c2 + 1, ',');
      if (!c3)
      {
        read++;
        continue;
      }
      char *c4 = strchr(c3 + 1, ',');
      *c1 = '\0';
      unsigned long ft;
      if (c4)
      {
        *c4 = '\0';
        ft = strtoul(c3 + 1, nullptr, 10);
      }
      else
      {
        ft = strtoul(c3 + 1, nullptr, 10);
      }
      if (strcmp(tmp, rfid) == 0 && ft > 0 && t >= ft && (t - ft) < MIN_REPEAT_INTERVAL)
      {
        found = true;
        break;
      }
      read++;
    }
    file.close();
    if (found)
      return true;
  }
  return false;
}

static bool looksLikeTimestamp(const char *s)
{
  for (int i = 0; i < 19; i++)
  {
    char c = s[i];
    if (i == 4 || i == 7)
    {
      if (c != '-')
        return false;
    }
    else if (i == 10)
    {
      if (c != ' ')
        return false;
    }
    else if (i == 13 || i == 16)
    {
      if (c != ':')
        return false;
    }
    else if (c < '0' || c > '9')
      return false;
  }
  return true;
}

static int classifyQueueLine(char *line, OfflineRecord *out, time_t now, bool nowValid)
{
  int len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    line[--len] = '\0';
  if (len == 0)
    return LINE_EMPTY;
  if (len < 10)
    return LINE_MALFORMED;
  char *c1 = strchr(line, ',');
  char *c2 = c1 ? strchr(c1 + 1, ',') : nullptr;
  char *c3 = c2 ? strchr(c2 + 1, ',') : nullptr;
  char *c4 = c3 ? strchr(c3 + 1, ',') : nullptr;
  if (!c1 || !c2 || !c3)
    return LINE_MALFORMED;
  int rl = c1 - line, tl = c2 - c1 - 1, dl = c3 - c2 - 1;

  if (rl != 10 || tl != 19 || dl > 19 || !looksLikeTimestamp(c1 + 1))
    return LINE_MALFORMED;
  unsigned long recT = strtoul(c3 + 1, nullptr, 10);
  if (recT == 0)
    return LINE_MALFORMED;
  if (c4)
  {
    char rfidTmp[11];
    memset(rfidTmp, 0, sizeof(rfidTmp));
    memcpy(rfidTmp, line, rl);
    char expected[3];
    snprintf(expected, sizeof(expected), "%02X", (unsigned)recordCrc8(rfidTmp, recT));
    if (strncmp(c4 + 1, expected, 2) != 0 || c4[3] != '\0')
      return LINE_BAD_CRC;
  }
  if (isRecordExpired(recT, now, nowValid))
    return LINE_EXPIRED;
  if (out)
  {
    memset(out, 0, sizeof(OfflineRecord));
    memcpy(out->rfid, line, rl);
    memcpy(out->timestamp, c1 + 1, tl);
    memcpy(out->deviceId, c2 + 1, dl);
    out->unixTime = recT;
  }
  return LINE_OK;
}
