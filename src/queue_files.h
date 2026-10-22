#pragma once
void getQueueFileName(int idx, char *buf, size_t sz)
{
  snprintf(buf, sz, "/queue_%d.csv", idx);
}

static bool findNextQueueFileLocked(int fromIdx, int *outIdx)
{
  FsFile root, entry;
  if (!root.open("/"))
    return false;
  int best = -1;
  char name[40];
  while (entry.openNext(&root, O_RDONLY))
  {
    esp_task_wdt_reset();
    taskYIELD();
    bool isFile = entry.isFile();
    if (isFile)
      entry.getName(name, sizeof(name));
    entry.close();
    if (!isFile)
      continue;
    int idx;
    if (parseQueueIndex(name, &idx) && idx >= fromIdx && (best < 0 || idx < best))
      best = idx;
  }
  root.close();
  if (best < 0)
    return false;
  *outIdx = best;
  return true;
}

int countRecordsInFileLocked(const char *filename)
{
  if (!file.open(filename, O_RDONLY))
    return 0;
  int cnt = 0;
  char line[128];
  if (file.available())
    file.fgets(line, sizeof(line));
  while (file.fgets(line, sizeof(line)) > 0)
  {
    esp_task_wdt_reset();
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len > 10)
      cnt++;
  }
  file.close();
  return cnt;
}

void saveMetadataLocked()
{
  selectSD();
  if (file.open(METADATA_FILE, O_WRONLY | O_CREAT | O_TRUNC))
  {
    file.print(cachedPendingRecords);
    file.print(',');
    file.println(currentQueueFile);
    file.sync();
    file.close();
  }
  deselectSD();
}

void loadMetadataLocked()
{
  selectSD();
  if (!sd.exists(METADATA_FILE))
  {
    deselectSD();
    return;
  }
  if (!file.open(METADATA_FILE, O_RDONLY))
  {
    deselectSD();
    return;
  }
  char line[32];
  if (file.fgets(line, sizeof(line)) > 0)
  {
    char *comma = strchr(line, ',');
    if (comma)
    {
      *comma = '\0';
      cachedPendingRecords = atoi(line);
      currentQueueFile = atoi(comma + 1);
      rtcQueueFileValid = true;
      pendingCacheDirty = false;
    }
  }
  file.close();
  deselectSD();
}
