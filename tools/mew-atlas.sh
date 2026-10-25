#!/system/bin/sh
# Mewgenics (PPSA37318): фиксация размера атласа растровых заливок SWF (BitmapFillAtlas) на 4096x4096.
# Предотвращает вылет при загрузке вступительных роликов глав (Стоки, Бункер, Кратер и др.),
# в которых растровые картинки имеют размер до 3641x1956 и не влезают при значении 2048
# (из-за texture_atlas_size 2048 в settings.txt летит исключение "Bitmap Fill could not fit into atlas").
# Правка заменяет вызов чтения настройки на фиксацию eax = 4096 (0x1000).
# Запуск (игра закрыта): su -c 'sh mew-atlas.sh'      откат: su -c 'sh mew-atlas.sh undo'
G=/data/data/org.anyps5.android/files/games/PPSA37318
CACHE=/data/data/org.anyps5.android/files/.cache/box64
cd "$G" || { echo "нет $G"; exit 1; }
if pidof org.anyps5.android > /dev/null; then echo "игра запущена: app.elf открыт, правка только при закрытой игре"; exit 1; fi
drop_cache() { rm -f "$CACHE"/app.elf-*.box64 && echo "кеш трансляции app.elf удалён"; }
if [ "$1" = undo ]; then
  [ -f app.elf.pre-atlas ] && cp -p app.elf.pre-atlas app.elf && echo "возвращён app.elf до патча атласа" && drop_cache || echo "нет app.elf.pre-atlas"
  exit 0
fi
SITES='
15671154 e8b91dfaff b800100000 \270\000\020\000\000
'
bytes_at() { od -An -tx1 -j "$1" -N"$2" app.elf | tr -d ' \n'; }
printf '%s\n' "$SITES" | while read -r off old new oct; do
  [ -z "$off" ] && continue
  cur=$(bytes_at "$off" $(( ${#old} / 2 )))
  if [ "$cur" = "$new" ]; then echo "$off: уже исправлено"
  elif [ "$cur" = "$old" ]; then echo "$off: будет исправлено"
  else echo "$off: НЕОЖИДАННЫЕ байты $cur"; fi
done > /data/local/tmp/mew-atlas.check
cat /data/local/tmp/mew-atlas.check
grep -q "НЕОЖИДАННЫЕ" /data/local/tmp/mew-atlas.check && { echo "ничего не изменено: другая сборка app.elf"; exit 1; }
grep -q "будет исправлено" /data/local/tmp/mew-atlas.check || { echo "всё уже исправлено"; exit 0; }
[ -f app.elf.pre-atlas ] || cp -p app.elf app.elf.pre-atlas
printf '%s\n' "$SITES" | while read -r off old new oct; do
  [ -z "$off" ] && continue
  [ "$(bytes_at "$off" $(( ${#old} / 2 )))" = "$old" ] && printf "$oct" | dd of=app.elf bs=1 seek="$off" conv=notrunc 2>/dev/null
done
printf '%s\n' "$SITES" | while read -r off old new oct; do
  [ -z "$off" ] && continue
  [ "$(bytes_at "$off" $(( ${#old} / 2 )))" = "$new" ] || echo "$off: НЕ ЗАПИСАЛОСЬ"
done
drop_cache
echo "готово (копия исходного файла: app.elf.pre-atlas)"
