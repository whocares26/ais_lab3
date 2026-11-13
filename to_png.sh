# SVG -> PNG для вставки в записку (нужен rsvg-convert из пакета librsvg2-bin)
cd "$(dirname "$0")/plots"
for f in *.svg; do
    rsvg-convert -b white -z 2 "$f" -o "${f%.svg}.png" && echo "$f -> ${f%.svg}.png"
done
