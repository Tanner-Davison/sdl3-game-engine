#!/bin/bash
echo "Generating Documentation..."
doxygen MyDoxyFile
echo "Done! Open docs/html/index.html"

if [ ! -f "docs/html/index.html" ]; then
    echo "Documentation not found! Generating..."
    doxygen
fi

echo "Opening File Documentation in Browser using wslview"
wslview docs/html/index.html
echo "Done! Viewing Documentation in Browser!"
