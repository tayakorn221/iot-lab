#include "telemetry.h"

#include <stdarg.h>
#include <stdio.h>

namespace {

// ต่อข้อความท้าย buf แบบไม่ล้น · ล้นเมื่อไหร่ ok = false ไปตลอด
struct Writer {
  char* buf;
  size_t len;
  size_t pos = 0;
  bool ok = true;

  Writer(char* b, size_t l) : buf(b), len(l) {}

  void add(const char* fmt, ...) {
    if (!ok) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + pos, len - pos, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= len - pos) {
      ok = false;
      return;
    }
    pos += n;
  }

  void addFloat(const char* key, float v, int decimals) {
    if (isfinite(v)) add(",\"%s\":%.*f", key, decimals, (double)v);
  }

  void addInt(const char* key, int v) {
    if (v >= 0) add(",\"%s\":%d", key, v);
  }
};

}  // namespace

size_t telemetryToJson(const Telemetry& t, char* buf, size_t len) {
  if (len == 0) return 0;
  Writer w{buf, len};
  w.add("{\"ts\":%lu,\"seq\":%lu", (unsigned long)t.ts, (unsigned long)t.seq);
  // ทศนิยมเท่าที่เซนเซอร์แยกได้จริง — เกินนี้เป็นแค่ noise
  w.addFloat("temp_c", t.temp_c, 2);
  w.addFloat("rh_pct", t.rh_pct, 1);
  w.addFloat("pres_hpa", t.pres_hpa, 1);
  w.addInt("pm1_ugm3", t.pm1_ugm3);
  w.addInt("pm25_ugm3", t.pm25_ugm3);
  w.addInt("pm10_ugm3", t.pm10_ugm3);
  w.add("}");
  if (!w.ok) {
    buf[0] = '\0';
    return 0;
  }
  return w.pos;
}
