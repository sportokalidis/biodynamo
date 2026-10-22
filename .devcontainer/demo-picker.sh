#!/bin/bash
# Interactive demo picker for BioDynaMo Codespaces
DEMO_DIR="/workspaces/biodynamoOnline/demo"

echo ""
echo "========================================="
echo "  Welcome to BioDynaMo Demo Environment"
echo "========================================="
echo ""
echo "Available demos:"
echo ""

# Build list of demos
demos=()
i=1
for d in "$DEMO_DIR"/*/; do
  name=$(basename "$d")
  demos+=("$name")
  printf "  %2d) %s\n" "$i" "$name"
  i=$((i + 1))
done

echo ""
read -rp "Select a demo [1-${#demos[@]}] (or press Enter to skip): " choice

if [[ -n "$choice" ]] && [[ "$choice" -ge 1 ]] && [[ "$choice" -le "${#demos[@]}" ]]; then
  selected="${demos[$((choice - 1))]}"
  target="$DEMO_DIR/$selected"
  cd "$target" || exit 1
  echo ""
  echo "Changed to demo/$selected"

  if [ -f README.md ]; then
    code --command markdown.showPreview README.md
  fi

  echo "Run: bdm build && bdm run"
  echo ""
  exec bash
else
  echo "Skipped. You can navigate to any demo in demo/"
  echo ""
fi
