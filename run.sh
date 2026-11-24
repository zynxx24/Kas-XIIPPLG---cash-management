#!/bin/bash
# ═══════════════════════════════════════════════════════
#  XII PPLG Cash Management - Runner Script
#  Includes: Compile, Run Server, Cloudflare Tunnel
# ═══════════════════════════════════════════════════════

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVER_DIR="$PROJECT_DIR/server"
PORT=8080

print_banner() {
    echo -e "${CYAN}"
    echo "╔══════════════════════════════════════════════════╗"
    echo "║     XII PPLG CASH MANAGEMENT                    ║"
    echo "║     Runner + Cloudflare Tunnel                  ║"
    echo "╚══════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

cleanup() {
    echo -e "\n${YELLOW}[SHUTDOWN] Menghentikan semua proses...${NC}"
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null
        echo -e "${GREEN}[OK] Server dihentikan${NC}"
    fi
    if [ ! -z "$TUNNEL_PID" ]; then
        kill $TUNNEL_PID 2>/dev/null
        echo -e "${GREEN}[OK] Cloudflare Tunnel dihentikan${NC}"
    fi
    exit 0
}

trap cleanup SIGINT SIGTERM

# ── Step 1: Compile Server ──
compile_server() {
    echo -e "${BOLD}[1/3] Kompilasi server C...${NC}"
    cd "$SERVER_DIR"
    make clean > /dev/null 2>&1 || true
    make 2>&1 | tail -1
    echo -e "${GREEN}[OK] Server berhasil dikompilasi${NC}\n"
}

# ── Step 2: Start Server ──
start_server() {
    echo -e "${BOLD}[2/3] Menjalankan server di port $PORT...${NC}"
    # Stop existing process on port 8080 or running binary if any
    fuser -k ${PORT}/tcp > /dev/null 2>&1 || true
    pkill -x server > /dev/null 2>&1 || true
    sleep 0.5

    cd "$SERVER_DIR"
    ./server &
    SERVER_PID=$!
    sleep 1

    # Quick health check
    if kill -0 $SERVER_PID 2>/dev/null && curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT/ | grep -q "200"; then
        echo -e "${GREEN}[OK] Server aktif di http://localhost:$PORT${NC}\n"
    else
        echo -e "${RED}[ERROR] Server gagal start!${NC}"
        exit 1
    fi
}

# ── Step 3: Cloudflare Tunnel ──
install_cloudflared() {
    if command -v cloudflared &> /dev/null; then
        echo -e "${GREEN}[OK] cloudflared sudah terinstall$(cloudflared --version 2>/dev/null | head -1)${NC}"
        return 0
    fi

    echo -e "${YELLOW}[INFO] cloudflared belum terinstall. Menginstall...${NC}"

    # Detect architecture
    ARCH=$(uname -m)
    case $ARCH in
        x86_64)  CF_ARCH="amd64" ;;
        aarch64) CF_ARCH="arm64" ;;
        armv7l)  CF_ARCH="arm"   ;;
        *)
            echo -e "${RED}[ERROR] Arsitektur $ARCH tidak didukung${NC}"
            exit 1
            ;;
    esac

    # Download and install
    CF_URL="https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-$CF_ARCH"
    echo -e "${CYAN}[DOWNLOAD] $CF_URL${NC}"

    if sudo -n true 2>/dev/null; then
        sudo curl -sL "$CF_URL" -o /usr/local/bin/cloudflared
        sudo chmod +x /usr/local/bin/cloudflared
    else
        mkdir -p "$HOME/.local/bin"
        curl -sL "$CF_URL" -o "$HOME/.local/bin/cloudflared"
        chmod +x "$HOME/.local/bin/cloudflared"
        export PATH="$HOME/.local/bin:$PATH"
    fi

    if command -v cloudflared &> /dev/null; then
        echo -e "${GREEN}[OK] cloudflared berhasil diinstall${NC}"
    else
        echo -e "${RED}[ERROR] Gagal install cloudflared${NC}"
        exit 1
    fi
}

start_tunnel() {
    echo -e "${BOLD}[3/3] Membuka Cloudflare Tunnel...${NC}"
    install_cloudflared

    echo -e "${CYAN}[TUNNEL] Menghubungkan ke Cloudflare...${NC}"
    echo -e "${YELLOW}         Tunggu beberapa detik sampai URL muncul...${NC}\n"

    LOG_FILE="/tmp/cloudflared_pplg.log"
    rm -f "$LOG_FILE"

    cloudflared tunnel --url http://localhost:$PORT > "$LOG_FILE" 2>&1 &
    TUNNEL_PID=$!

    # Wait for tunnel URL
    TRIES=0
    TUNNEL_URL=""
    while [ $TRIES -lt 30 ]; do
        sleep 1.5
        if [ -f "$LOG_FILE" ]; then
            TUNNEL_URL=$(grep -o "https://[-0-9a-z]*\.trycloudflare\.com" "$LOG_FILE" | head -n1 || echo "")
            if [ ! -z "$TUNNEL_URL" ]; then
                break
            fi
        fi
        TRIES=$((TRIES + 1))
    done

    echo ""
    echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  ✅ Server + Tunnel berjalan!${NC}"
    echo -e "${GREEN}  📍 Local:  http://localhost:$PORT${NC}"
    if [ ! -z "$TUNNEL_URL" ]; then
        echo -e "${GREEN}  🌐 Public: ${BOLD}${TUNNEL_URL}${NC}"
    else
        echo -e "${RED}  ❌ Gagal mendeteksi URL Publik. Coba cek dengan run manual.${NC}"
    fi
    echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${YELLOW}  Tekan Ctrl+C untuk menghentikan semua${NC}"
    echo ""
}

# ── Main ──
print_banner

case "${1:-tunnel}" in
    local)
        echo -e "${YELLOW}Mode: Local only (tanpa tunnel)${NC}\n"
        compile_server
        start_server
        echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
        echo -e "${GREEN}  ✅ Server berjalan!${NC}"
        echo -e "${GREEN}  📍 Local: http://localhost:$PORT${NC}"
        echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
        echo -e "${YELLOW}  Tekan Ctrl+C untuk menghentikan${NC}"
        wait $SERVER_PID
        ;;
    tunnel|*)
        if [ ! -f "$SERVER_DIR/.ENV" ] && [ ! -f "$PROJECT_DIR/.ENV" ]; then
            echo -e "${RED}[ERROR] server/.ENV belum ada. Tanpa file itu server memakai password admin bawaan.${NC}"
            echo -e "${YELLOW}        Salin server/.ENV.example ke server/.ENV dan isi passwordnya, atau jalankan: ./run.sh local${NC}"
            exit 1
        fi
        echo -e "${YELLOW}Mode: Cloudflare Tunnel (public)${NC}\n"
        compile_server
        start_server
        start_tunnel
        wait $TUNNEL_PID
        ;;
esac
