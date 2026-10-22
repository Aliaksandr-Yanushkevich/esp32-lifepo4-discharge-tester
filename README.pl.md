# Tester rozładowania ogniw LiFePO4 na ESP32-C3

[English](README.md) | [Polski](README.pl.md)

Prototyp testera rozładowania pojedynczego ogniwa LiFePO4, zbudowany na ESP32-C3 Mini, z obciążeniem rezystancyjnym i tranzystorem MOSFET. Sterowanie odbywa się przez Telegram — wyświetlacz nie jest potrzebny.

Oprogramowanie znajduje się w pliku [`lifepo4_tester.ino`](lifepo4_tester.ino). Mierzy napięcie ogniwa i przełącza obciążenie według wybranego progu odcięcia. **Nie mierzy prądu ani nie oblicza pojemności lub energii (Ah/Wh).**

## Dlaczego powstał ten projekt

Do innego projektu potrzebowałem wybrać ogniwa o jak najmniejszej rezystancji wewnętrznej i jak największej pojemności. Dostępny sprzęt miał ograniczoną moc rozładowania, przez co rozładowywanie ogniw trwało zbyt długo.

Aby rozładowywać je szybciej i bezpieczniej, zbudowałem ten prototyp z elementów, które miałem już na półce, wykorzystując zewnętrzne rezystory ceramiczne jako obciążenie. Było to doraźne rozwiązanie konkretnego problemu, a nie próba stworzenia najlepszego testera rozładowania. Układ miał pomóc na etapie rozładowywania ogniw podczas ich selekcji; sam prototyp nie mierzy rezystancji wewnętrznej ani pojemności.

## Elementy i połączenia

| Element | Wartość / zastosowanie |
| --- | --- |
| ESP32-C3 Mini | Sterownik |
| MOSFET IRLB8743 z kanałem N | Przełączanie obciążenia rezystancyjnego |
| Rezystor(y) obciążenia | Rezystancję i moc znamionową należy dobrać do planowanego obciążenia |
| Dzielnik napięcia | Dwa rezystory 100 kΩ |
| Kondensator filtra ADC | 0,1 µF między wejściem ADC a masą |
| Rezystor szeregowy bramki | 100 Ω |
| Rezystor ściągający bramkę do masy | 100 kΩ |

| Pin ESP32 | Połączenie |
| --- | --- |
| GPIO0 | Punkt środkowy dzielnika napięcia (ADC) |
| GPIO5 | Bramka MOSFET-a przez rezystor 100 Ω |
| GND | Minus ogniwa i źródło MOSFET-a |

```text
Obwód obciążenia:

BAT+ ---- Rezystor(y) obciążenia ---- Dren (MOSFET)
                                    Źródło ---- GND ---- BAT-

Pomiar napięcia:

BAT+ ---- 100 kΩ ----+---- 100 kΩ ---- GND
                    |
                  GPIO0
                    |
                  0,1 µF
                    |
                   GND

Sterowanie bramką:

GPIO5 ---- 100 Ω ---- Bramka
                     |
                   100 kΩ
                     |
                    GND
```

Nominalny współczynnik dzielnika wynosi 2. Wszystkie masy muszą być ze sobą połączone. Schemat przedstawia połączenia pomiarowe i obciążenie; nie określa sposobu zasilania ESP32. Należy użyć zasilania odpowiedniego dla konkretnej płytki.

## Przygotowanie oprogramowania

1. Zainstaluj pakiet obsługi płytek ESP32 w Arduino IDE i wybierz płytkę oraz port odpowiadające używanemu ESP32-C3.
2. Zainstaluj biblioteki **UniversalTelegramBot** i **ArduinoJson**. Biblioteki `WiFi`, `WiFiClientSecure` i `Preferences` są dostarczane z pakietem ESP32.
3. Umieść plik `lifepo4_tester.ino` w folderze szkicu o nazwie `lifepo4_tester` i otwórz go w Arduino IDE.
4. Zastąp wartości zastępcze na początku szkicu:

   | Ustawienie | Wartość |
   | --- | --- |
   | `WIFI_SSID` | Nazwa sieci Wi-Fi |
   | `WIFI_PASSWORD` | Hasło sieci Wi-Fi |
   | `BOT_TOKEN` | Token bota Telegram |
   | `CHAT_ID` | Identyfikator uprawnionego czatu Telegram, jako ciąg znaków |

5. Sprawdź połączenia i kalibrację napięcia przed użyciem obciążenia, a następnie wgraj szkic.
6. Otwórz monitor portu szeregowego z prędkością **115200 bodów**, aby odczytać komunikaty połączenia i diagnostykę.

Nie zapisuj prawdziwych danych dostępowych w commitach. Repozytorium nie określa wymaganych wersji pakietu ESP32 ani bibliotek.

## Przebieg rozładowania

| Ustawienie | Wartość domyślna |
| --- | --- |
| Próg odcięcia | 3,00 V, jeśli nie ma zapisanej wartości |
| Histereza ponownego załączenia | 0,05 V |
| Interwał sterowania | 500 ms, gdy pętla nie jest zablokowana |
| Interwał sprawdzania wiadomości Telegram | 1200 ms, gdy pętla nie jest zablokowana |
| Interwał diagnostyki szeregowej | 5 sekund, gdy pętla nie jest zablokowana |

- Po uruchomieniu wyjście sterujące MOSFET-em jest ustawiane na LOW, a rozładowanie jest wyłączone. Poprzednia sesja rozładowania nie jest wznawiana.
- **Start** włącza tryb rozładowania i kasuje blokadę odcięcia. Następny cykl sterowania włącza MOSFET bez wcześniejszego sprawdzenia, czy napięcie jest powyżej progu odcięcia.
- Gdy MOSFET jest włączony, odczyt napięcia równego progowi odcięcia lub niższego wyłącza go i ustawia blokadę odcięcia.
- Po odcięciu tryb rozładowania pozostaje aktywny. Jeśli napięcie wzrośnie **powyżej progu odcięcia + 0,05 V**, obciążenie włączy się automatycznie. Wzrost napięcia ogniwa po odłączeniu obciążenia może więc powodować powtarzające się cykle odcięcia i wznowienia.
- **Stop** wyłącza tryb rozładowania i MOSFET po przetworzeniu polecenia.
- Wybrany próg odcięcia jest zapisywany w pamięci nieulotnej ESP32 (przestrzeń nazw `tester`, klucz `cutoff`). Obowiązuje w kolejnych cyklach sterowania i jest zachowywany po restarcie. Kalibracja i histereza są zdefiniowane w szkicu.

Pomimo nazwy stanu `CUTOFF_LATCHED` odcięcie nie oznacza trwałego zatrzymania wymagającego ponownego polecenia Start.

## Sterowanie przez Telegram

Akceptowane są wyłącznie wiadomości z czatu określonego przez `CHAT_ID`. Sprawdzany jest identyfikator czatu; w przypadku uprawnionego czatu grupowego nie są sprawdzani poszczególni nadawcy.

Nazwy przycisków w bocie pozostają w języku angielskim.

| Przycisk | Polecenie | Działanie |
| --- | --- | --- |
| ▶ Start | `/start` | Włącz tryb rozładowania i skasuj blokadę odcięcia |
| ⏹ Stop | `/stop` | Wyłącz tryb rozładowania i obciążenie |
| 📊 Status | `/status` | Pokaż napięcie, próg odcięcia, stan oraz flagi MOSFET-a i blokady |
| 🧪 Diag | `/diag` | Pokaż odczyty ADC, kalibrację i flagi sterowania |
| ⚙ Cutoff | Tylko przycisk | Otwórz wybór progu odcięcia |
| ❓ Help | `/help` | Pokaż opis sterowania i wartości kalibracji |
| ⬅ Back | Tylko przycisk | Wróć do menu głównego |

**Wysłanie `/start` uruchamia rozładowanie**, również wtedy, gdy wiadomość `/start` służy do rozpoczęcia nowej rozmowy z botem.

Dostępne progi odcięcia: **2,80; 2,90; 3,00; 3,05; 3,10; 3,20 i 3,30 V**. Dobierz próg do ogniwa i warunków testu, zgodnie ze specyfikacją producenta ogniwa.

Bot wysyła komunikaty po uruchomieniu urządzenia, odpowiedzi na polecenia oraz powiadomienia o odcięciu i automatycznym wznowieniu. Nie wysyła okresowych raportów stanu; powtarzające się cykle odcięcia i wznowienia mogą jednak generować kolejne powiadomienia.

## Pomiar napięcia i kalibracja

Każdy pomiar napięcia jest średnią z 64 surowych próbek ADC, z opóźnieniem 300 µs między próbkami. ADC jest skonfigurowany na rozdzielczość 12 bitów i tłumienie `ADC_11db`.

Szkic oblicza napięcie w następujący sposób (w zapisie kodu separatorem dziesiętnym jest kropka):

```text
V_adc      = ADC_average / 4095 × 3.3
V_measured = V_adc × 2
V_reported = calibA × V_measured + calibB

calibA = 0.8938
calibB = 0.0000
```

Wartość odniesienia 3,3 V jest przybliżeniem używanym w kodzie, a nie zmierzonym napięciem odniesienia ADC. Podane współczynniki kalibracji należy sprawdzić multimetrem na własnym urządzeniu. Aby je zmienić, edytuj `calibA` i `calibB` w szkicu i wgraj go ponownie. Telegram nie udostępnia polecenia kalibracji.

Diagnostyka szeregowa i przycisk **Diag** w Telegramie pokazują uśrednioną surową wartość ADC, oszacowane napięcie ADC, napięcie ogniwa przed kalibracją i po niej, próg odcięcia, histerezę, flagi trybu rozładowania, MOSFET-a i blokady oraz współczynniki kalibracji.

## Ograniczenia prototypu

- **Operacje sieciowe blokują sterowanie rozładowaniem.** Łączenie i ponowne łączenie z Wi-Fi jest ponawiane bez końca. W tym czasie pętla nie sprawdza napięcia, a wcześniej włączony MOSFET pozostaje włączony. Zapytania do Telegrama i obsługa wiadomości również mogą opóźniać sprawdzanie napięcia oraz wykonanie polecenia Stop.
- **Start nie odrzuca ogniwa o napięciu poniżej progu odcięcia.** Może włączyć obciążenie do chwili wykrycia niskiego napięcia w kolejnym cyklu sterowania.
- **Blokada odcięcia jest automatycznie kasowana po wzroście napięcia.** Oprogramowanie nie kończy testu trwale przy pierwszym odcięciu.
- **Weryfikacja certyfikatu TLS jest wyłączona** przez `securedClient.setInsecure()`.
- Brak pomiaru prądu, obliczania pojemności, monitorowania temperatury, trwałego zapisu pomiarów oraz osobnego wykrywania awarii ADC lub układu pomiarowego.
- Układ jest przeznaczony do pojedynczego ogniwa; nie jest testerem pakietów ani obciążeniem elektronicznym o stałym prądzie.

Używaj niezależnego układu zabezpieczającego; sam szkic nie zapewnia niezawodnej ochrony ogniwa. Dobierz rezystory obciążenia, chłodzenie MOSFET-a i przewody do planowanego obciążenia, zastosuj odpowiedni bezpiecznik oraz sprawdź wskazania napięcia i działanie przełączania przed testem rozładowania.

## Możliwe ulepszenia

- Nieblokująca obsługa sieci, aby sterowanie napięciowe działało również przy problemach z połączeniem.
- Odrzucanie Start poniżej progu odcięcia i wymaganie jawnego ponownego uruchomienia po odcięciu.
- Pomiar prądu oraz obliczanie Ah/Wh przez całkowanie.
- Monitorowanie temperatury i wyłączenie w przypadku awarii.
- Sterowanie kalibracją i zapis pomiarów.

## Materiały

Zdjęcia: [udostępniony album iCloud](https://www.icloud.com/sharedalbum/#D1gv3kj9t14uM_-qoxNOJfSF_bACAEQARogxGbpijmXgZkVoKax6umBtObMlxxSzGx67AQJz0zXkXI).
