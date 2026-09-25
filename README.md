# RAUCH Fertilizer Server

Usługa odbiera 206-bajtowy rekord XF1 przez TCP i zapisuje go jako plik w
katalogu współdzielonym z serwerem ISO11783-13.

Protokół XF1 i format 206 bajtów są opisane w `../rauch/protocols/rauch-wlan`.


## Automatyczne can-utils

Na Linuksie CMake domyślnie pobiera `linux-can/can-utils` `v2025.01`, buduje
`isobusfs-srv` i kopiuje go obok aplikacji. can-utils używa linuksowego
J1939/SocketCAN i nie jest zależnością przenośną na Windows lub macOS.

Konfiguracja:

```bash
cmake -S . -B build -DRAUCH_FETCH_CAN_UTILS=ON
cmake --build build --target rauch-fertilizer-server isobusfs-srv
```

Hotspot jest domyślnie uruchamiany pod adresem `152.21.0.31/24`, a odbiornik
TCP nasłuchuje wyłącznie na `152.21.0.31:8172` — nie wystawia portu na pozostałe
interfejsy hosta. Najwygodniej używać nazwanych opcji — ich kolejność jest
dowolna: `--port`, `--output-dir`, `--can-interface`, `--j1939-address`,
`--isobusfs-executable`, `--listen-address`, `--no-hotspot` i `--no-isobusfs`.

```bash
./build/rauch-fertilizer-server 8172 /tmp/rauch-fs can0 f8
```

Równoważne wywołanie z opcjami:

```bash
./build/rauch-fertilizer-server --can-interface can0 --j1939-address f8 \
  --output-dir /tmp/rauch-fs --port 8172
```

Aby użyć zainstalowanego `isobusfs-srv` zamiast pobierania can-utils, ustaw
`-DRAUCH_FETCH_CAN_UTILS=OFF`; na Linuksie program szuka wtedy `isobusfs-srv` w
`PATH`. Można też podać ścieżkę przez `--isobusfs-executable PATH`.

Jeśli hotspot jest wyłączony, wybierz adres lokalny interfejsu, np.
`--listen-address 127.0.0.1`. Port `0` wybiera wolny port systemowy.


## Układ projektu

Wspólny kod aplikacji — nagłówki i implementacje — znajduje się razem w `app/`. Kod zależny od systemu operacyjnego jest wydzielony w `platform/unix`, `platform/macos` i `platform/windows`; CMake wybiera odpowiednią implementację podczas konfiguracji. Testy są w `tests/`.


Na Linuksie transfer ISO11783-13/J1939 realizuje zewnętrzny `isobusfs-srv` z
can-utils. `ICanTransport` to osobna, niskopoziomowa abstrakcja surowych ramek
CAN; nie jest obecnie używana do transportu ISO11783-13 przez aplikację.

Na Windows można zbudować zwykły EXE i użyć odbiornika TCP (`--no-isobusfs`),
ale aplikacja nie ma jeszcze przenośnej implementacji ISO11783-13. Domyślny
start kończy się czytelnym błędem, jeśli nie podano zgodnego programu
`isobusfs-srv`. Opcjonalny backend PCAN obsługuje ramki CAN standardowe i
rozszerzone, ale sam nie zastępuje transportu J1939 ani serwera plików. Hotspot
Windows używa starszego Hosted Network i wymaga sterownika, który ten tryb
udostępnia, oraz uprawnień administratora. macOS nie ma backendu hotspotu ani
ISO11783-13; użyj `--no-hotspot --no-isobusfs` w trybie TCP-only.


## Hotspot RAUCH (Linux)

Na Linuksie z NetworkManagerem aplikacja wybiera pierwszą dostępną kartę Wi-Fi
i uruchamia hotspot `quantron` / `quantron` z adresem `152.21.0.31/24`.
Współdzielenie NetworkManagera zapewnia klientom DHCP. Wymagane są `nmcli` i
karta obsługująca tryb AP. Hotspot rozłączy tę kartę z jej dotychczasową siecią;
przy zamknięciu aplikacja usuwa swój profil i próbuje przywrócić poprzednie
połączenie. Wywołania `nmcli` wymagają odpowiednich uprawnień PolicyKit.

```bash
sudo ./build/rauch-fertilizer-server 8172 /tmp/rauch-fs vcan0 80
```

Hotspot uruchamia się domyślnie; opcja `--no-hotspot` wyłącza go. Windows używa natywnego Hosted Network i DHCP ICS; wymaga uruchomienia jako administrator oraz adaptera/sterownika, który obsługuje starszy tryb Hosted Network (wielu nowych sterowników go nie udostępnia). Aplikacja przydziela wirtualnemu adapterowi także `152.21.0.31/24` i usuwa ten adres przy zamknięciu. macOS nie ma jeszcze backendu hotspotu; uruchom tam aplikację z `--no-hotspot`.
