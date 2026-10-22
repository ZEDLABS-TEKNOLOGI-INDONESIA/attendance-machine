#pragma once
static bool openQuarantineLocked(FsFile &q)
{
  if (sd.exists(QUARANTINE_FILE))
  {
    FsFile t;
    if (t.open(QUARANTINE_FILE, O_RDONLY))
    {
      uint64_t sz = t.size();
      t.close();
      if (sz > QUARANTINE_MAX_BYTES)
      {
        if (sd.exists(QUARANTINE_OLD))
          sd.remove(QUARANTINE_OLD);
        sd.rename(QUARANTINE_FILE, QUARANTINE_OLD);
      }
    }
  }
  if (!q.open(QUARANTINE_FILE, O_RDWR | O_CREAT | O_APPEND))
    return false;
  if (!prepareAppendLocked(q, "reason,source,raw_line"))
  {
    q.close();
    return false;
  }
  return true;
}

static bool quarantineRejectedLocked(const char *fn, int sentCnt, uint64_t keepMask)
{
  FsFile src;
  if (!src.open(fn, O_RDONLY))
    return false;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  FsFile q;
  bool qOpen = false, ok = true;
  int okOrd = 0;
  char line[144];
  if (src.available())
    src.fgets(line, sizeof(line));
  while (src.available())
  {
    esp_task_wdt_reset();
    if (src.fgets(line, sizeof(line)) <= 0)
      break;
    int rc = classifyQueueLine(line, nullptr, now, nowValid);
    const char *reason = nullptr;
    if (rc == LINE_OK)
    {
      int ord = okOrd++;
      if (ord >= sentCnt)
        reason = "UNSENT";
      else if (ord < 64 && ((keepMask >> ord) & 1ULL))
        continue;
    }
    else if (rc == LINE_EXPIRED)
      reason = "EXPIRED";
    else if (rc == LINE_BAD_CRC)
      reason = "BAD_CRC";
    else if (rc == LINE_MALFORMED)
      reason = "MALFORMED";
    if (!reason)
      continue;
    if (!qOpen)
    {
      if (!openQuarantineLocked(q))
      {
        ok = false;
        break;
      }
      qOpen = true;
    }
    char outLn[200];
    int n = snprintf(outLn, sizeof(outLn), "%s,%s,%s\r\n", reason, fn, line);
    if (n <= 0 || n >= (int)sizeof(outLn) || q.write(outLn, (size_t)n) != (size_t)n)
    {
      ok = false;
      break;
    }
  }
  src.close();
  if (qOpen)
  {
    if (ok)
      q.sync();
    q.close();
  }
  return ok;
}

static bool removeQueueFileSafeLocked(const char *fn, int sentCnt)
{
  if (!quarantineRejectedLocked(fn, sentCnt, 0))
    return false;
  return sd.remove(fn);
}

static bool keepRetryRecordsLocked(const char *fn, int sentCnt, uint64_t keepMask)
{
  if (!quarantineRejectedLocked(fn, sentCnt, keepMask))
    return false;
  const char *tmp = "/queue_keep.tmp";
  FsFile src, dst;
  if (!src.open(fn, O_RDONLY))
    return false;
  if (sd.exists(tmp))
    sd.remove(tmp);
  if (!dst.open(tmp, O_WRONLY | O_CREAT | O_TRUNC))
  {
    src.close();
    return false;
  }
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  bool ok = dst.println(QUEUE_HEADER_LINE) > 0;
  int okOrd = 0;
  char line[144];
  if (src.available())
    src.fgets(line, sizeof(line));
  while (ok && src.available())
  {
    esp_task_wdt_reset();
    if (src.fgets(line, sizeof(line)) <= 0)
      break;
    if (classifyQueueLine(line, nullptr, now, nowValid) != LINE_OK)
      continue;
    int ord = okOrd++;
    if (ord < 64 && ((keepMask >> ord) & 1ULL))
    {
      char out[160];
      int n = snprintf(out, sizeof(out), "%s\r\n", line);
      if (n <= 0 || n >= (int)sizeof(out) || dst.write(out, (size_t)n) != (size_t)n)
        ok = false;
    }
  }
  src.close();
  if (ok)
    dst.sync();
  dst.close();
  if (!ok)
  {
    sd.remove(tmp);
    return false;
  }
  if (!sd.remove(fn))
    return false;
  return sd.rename(tmp, fn);
}

static void recoverKeepTmpLocked()
{
  const char *tmp = "/queue_keep.tmp";
  if (!sd.exists(tmp))
    return;
  char fn[24];
  for (int i = 0; i < MAX_QUEUE_FILES; i++)
  {
    esp_task_wdt_reset();
    getQueueFileName(i, fn, sizeof(fn));
    if (sd.exists(fn))
      continue;
    if (sd.rename(tmp, fn))
      LOG_I("[SD] pulihkan %s dari queue_keep.tmp\n", fn);
    return;
  }
}
