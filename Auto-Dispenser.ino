#include <LiquidCrystal.h>
#include <EEPROM.h>

// Definições de pinos para LCD e botões
const int pinSelect = A0;
const int pinReset = 11;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Definições de pinos para o sensor de fluxo e válvula solenóide
const int flowSensorPin = 2;
const int relayPin = 3;
const int ledPin = 13; // Pino do LED embutido

// Variáveis de controle
volatile int pulseCount = 0;
unsigned long lastUpdateTime;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long totalFlowAccumulated = 0; // Fluxo total acumulado
unsigned long updateInterval = 1000; // Intervalo de atualização de 1 segundo
float flowRate;

// Variáveis para definir e monitorar o volume desejado
float desiredLitres = 0.0;
bool valveOpen = false;
bool settingLitres = true;
float incrementValue = 1.0; // Valor de incremento padrão definido para 1.0

// Variáveis de configuração
int filterNumber = 6000; // Número de filtro inicial definido para 6000
float kFactor = 7.5; // Fator K inicial

// Definições de botões
#define BUTTON_NONE 0
#define BUTTON_SELECT 1
#define BUTTON_LEFT 2
#define BUTTON_UP 3
#define BUTTON_DOWN 4
#define BUTTON_RIGHT 5

#define debounceTime 200

unsigned long lastButtonTime;

// Estado do menu
enum MenuState {
  MENU_INITIAL,
  MENU_DISPENSE,
  MENU_CONFIG,
  MENU_CONFIG_FILTER,
  MENU_CONFIG_INTERVAL,
  MENU_CONFIG_KFACTOR,
  MENU_RESET_COUNTER,
  MENU_SHOW_TOTAL
};
MenuState currentMenu = MENU_INITIAL;

const int menuLength = 2;
const int configLength = 5;

int menuIndex = 0; // Variável global para o índice do menu
int configIndex = 0; // Variável global para o índice do menu de configuração

void resetSystem();
void updateDisplay();
void checkButtons();
void processFlow();
void toggleValve();
void adjustLitres(float adjustment);
void loadConfig();
void saveConfig();
void displayInitialMenu();
void handleInitialMenu();
void displayConfigMenu();
void handleConfigMenu();
void displayConfigFilter();
void handleConfigFilter();
void displayConfigInterval();
void handleConfigInterval();
void displayConfigKFactor();
void handleConfigKFactor();
void displayResetCounter();
void handleResetCounter();
void displayTotalAccumulated();
void handleShowTotal();
void returnToInitialMenu();
void adjustMenuIndex(int direction, int menuLength);
void toggleIncrementValue();
void showIncrementMessage();

void setup() {
  // Inicialização dos pinos
  pinMode(pinSelect, INPUT);
  pinMode(pinReset, INPUT_PULLUP); // Configura o pino do botão de reset como entrada com pull-up interno
  pinMode(flowSensorPin, INPUT);
  digitalWrite(flowSensorPin, HIGH);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  pinMode(ledPin, OUTPUT); // Configura o pino do LED embutido como saída
  digitalWrite(ledPin, LOW); // Inicializa o LED como desligado

  // Inicialização do LCD
  lcd.begin(16, 2);

  // Configuração da interrupção para o sensor de fluxo
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  lastUpdateTime = millis();

  // Carrega as configurações da EEPROM
  loadConfig();

  // Mensagem de boas-vindas
  displayWelcome();
  delay(3000);
  displayInitialMenu();

  // Inicialização da comunicação serial para debug
  Serial.begin(9600);
  Serial.println("Sistema iniciado");
}

void loop() {
  checkButtons();

  switch (currentMenu) {
    case MENU_INITIAL:
      handleInitialMenu();
      break;
    case MENU_DISPENSE:
      if (digitalRead(pinReset) == LOW) {
        resetSystem();
      }
      if (millis() - lastUpdateTime > updateInterval) {
        processFlow();
        saveConfig(); // Periodicamente salva as configurações na EEPROM
      }
      break;
    case MENU_CONFIG:
      handleConfigMenu();
      break;
    case MENU_CONFIG_FILTER:
      handleConfigFilter();
      break;
    case MENU_CONFIG_INTERVAL:
      handleConfigInterval();
      break;
    case MENU_CONFIG_KFACTOR:
      handleConfigKFactor();
      break;
    case MENU_RESET_COUNTER:
      handleResetCounter();
      break;
    case MENU_SHOW_TOTAL:
      handleShowTotal();
      break;
    default:
      break;
  }
}

void resetSystem() {
  totalMilliLitres = 0;
  lcd.clear();
  lcd.print("System Reset");
  Serial.println("Sistema resetado");
  delay(2000);
  displayInitialMenu();
}

void updateDisplay() {
  lcd.setCursor(0, 0);
  lcd.print(totalMilliLitres / 1000.0, 1);
  lcd.print(" L    ");
  lcd.print(flowRate, 1);
  lcd.print(" L/m");

  lcd.setCursor(0, 1);
  lcd.print("Set:   ");
  lcd.print(desiredLitres, 1);
  lcd.print(" L    ");
}

void processFlow() {
  detachInterrupt(digitalPinToInterrupt(flowSensorPin));
  unsigned long currentMillis = millis();
  flowRate = ((1000.0 / (currentMillis - lastUpdateTime)) * pulseCount) / kFactor;
  lastUpdateTime = currentMillis;
  pulseCount = 0;
  flowMilliLitres = (flowRate / 60) * 1000;
  totalMilliLitres += flowMilliLitres;
  totalFlowAccumulated += flowMilliLitres; // Incrementa o fluxo total acumulado

  Serial.print("Fluxo: ");
  Serial.print(flowMilliLitres);
  Serial.print(" mL, Taxa de fluxo: ");
  Serial.print(flowRate);
  Serial.println(" L/m");

  updateDisplay();
  
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);

  // Verifica se o fluxo total acumulado atingiu o valor configurado do filtro
  if (totalFlowAccumulated >= filterNumber * 1000) { // Converte litros para mililitros
    lcd.setCursor(0, 1);
    lcd.print("Change Filter!   ");
    Serial.println("Atenção: Trocar o filtro!");
    delay(5000); // Exibe a mensagem por 5 segundos
    updateDisplay(); // Retorna a exibir os valores de fluxo
  }
}

void checkButtons() {
  int buttonValue = analogRead(pinSelect);

  if ((millis() - lastButtonTime) > debounceTime) {
    if ((buttonValue < 800) && (buttonValue >= 600)) { // Botão SELECT pressionado
      estadoBotao(BUTTON_SELECT);
    } else if ((buttonValue < 600) && (buttonValue >= 400)) { // Botão LEFT pressionado
      estadoBotao(BUTTON_LEFT);
    } else if ((buttonValue < 400) && (buttonValue >= 200)) { // Botão DOWN pressionado
      estadoBotao(BUTTON_DOWN);
    } else if ((buttonValue < 200) && (buttonValue >= 60)) { // Botão UP pressionado
      estadoBotao(BUTTON_UP);
    } else if (buttonValue < 60) { // Botão RIGHT pressionado
      estadoBotao(BUTTON_RIGHT);
    } else {
      estadoBotao(BUTTON_NONE);
    }
    lastButtonTime = millis();
  }
}

void estadoBotao(int botao) {
  switch (botao) {
    case BUTTON_SELECT:
      if (currentMenu == MENU_INITIAL) {
        if (menuIndex == 0) {
          currentMenu = MENU_DISPENSE;
          lcd.clear();
          updateDisplay();
        } else if (menuIndex == 1) {
          currentMenu = MENU_CONFIG;
          displayConfigMenu();
        }
      } else if (currentMenu == MENU_CONFIG) {
        if (configIndex == 0) {
          currentMenu = MENU_CONFIG_FILTER;
          displayConfigFilter();
        } else if (configIndex == 1) {
          currentMenu = MENU_CONFIG_INTERVAL;
          displayConfigInterval();
        } else if (configIndex == 2) {
          currentMenu = MENU_CONFIG_KFACTOR;
          displayConfigKFactor();
        } else if (configIndex == 3) {
          currentMenu = MENU_RESET_COUNTER;
          displayResetCounter();
        } else if (configIndex == 4) {
          currentMenu = MENU_SHOW_TOTAL;
          displayTotalAccumulated();
        }
      } else if (currentMenu == MENU_CONFIG_FILTER || currentMenu == MENU_CONFIG_INTERVAL || currentMenu == MENU_CONFIG_KFACTOR || currentMenu == MENU_RESET_COUNTER || currentMenu == MENU_SHOW_TOTAL) {
        currentMenu = MENU_CONFIG;
        displayConfigMenu();
      } else if (currentMenu == MENU_DISPENSE) {
        toggleValve();
        delay(2000); // Pequeno atraso para exibir a mensagem
        updateDisplay(); // Retorna a exibir os valores de fluxo
      }
      break;
    case BUTTON_LEFT:
      returnToInitialMenu();
      break;
    case BUTTON_UP:
      if (currentMenu == MENU_INITIAL || currentMenu == MENU_CONFIG) {
        adjustMenuIndex(-1, menuLength);
      } else if (currentMenu == MENU_DISPENSE) {
        adjustLitres(incrementValue);
      }
      break;
    case BUTTON_DOWN:
      if (currentMenu == MENU_INITIAL || currentMenu == MENU_CONFIG) {
        adjustMenuIndex(1, menuLength);
      } else if (currentMenu == MENU_DISPENSE) {
        adjustLitres(-incrementValue);
      }
      break;
    case BUTTON_RIGHT:
      if (currentMenu == MENU_DISPENSE) {
        toggleIncrementValue();
      }
      break;
    case BUTTON_NONE:
      // Nenhum botão pressionado
      break;
  }
}

void toggleIncrementValue() {
  if (incrementValue == 1.0) {
    incrementValue = 0.1;
  } else {
    incrementValue = 1.0;
  }
  showIncrementMessage();
}

void showIncrementMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Increment: ");
  lcd.print(incrementValue, 1);
  delay(1000); // Pequeno atraso para exibir a mensagem
  updateDisplay(); // Retorna a exibir os valores de fluxo
}

void toggleValve() {
  if (valveOpen) {
    digitalWrite(relayPin, LOW);
    digitalWrite(ledPin, LOW); // Desliga o LED embutido
    valveOpen = false;
    settingLitres = true;
    lcd.setCursor(0, 0);
    lcd.print("Flow OFF         ");
    Serial.println("Fluxo desligado");
  } else {
    digitalWrite(relayPin, HIGH);
    digitalWrite(ledPin, HIGH); // Liga o LED embutido
    valveOpen = true;
    settingLitres = false;
    lcd.setCursor(0, 0);
    lcd.print("Flow ON          ");
    Serial.println("Fluxo ligado");
  }
  delay(200); // Pequeno atraso para evitar múltiplas leituras
}

void adjustLitres(float adjustment) {
  if (settingLitres) {
    desiredLitres += adjustment;
    if (desiredLitres < 0) desiredLitres = 0;
    delay(200);
    updateDisplay();
    Serial.print("Litros desejados ajustados para: ");
    Serial.println(desiredLitres);
  }
}

void pulseCounter() {
  pulseCount++;
}

void loadConfig() {
  // Lê os valores da EEPROM
  EEPROM.get(0, totalFlowAccumulated);
  EEPROM.get(4, updateInterval);
  EEPROM.get(8, filterNumber);
  EEPROM.get(12, kFactor);

  // Verifica se os valores são válidos, caso contrário, inicializa com valores padrão
  if (updateInterval == 0xFFFFFFFF) {
    updateInterval = 1000;
  }
  if (filterNumber == -1) {
    filterNumber = 6000;
  }
  if (isnan(kFactor) || kFactor < 1.0 || kFactor > 20.0) {
    kFactor = 7.5;
  }

  Serial.println("Configurações carregadas da EEPROM:");
  Serial.print("Fluxo acumulado total: ");
  Serial.println(totalFlowAccumulated);
  Serial.print("Intervalo de atualização: ");
  Serial.println(updateInterval);
  Serial.print("Número do filtro: ");
  Serial.println(filterNumber);
  Serial.print("Fator K: ");
  Serial.println(kFactor);
}

void saveConfig() {
  // Salva os valores na EEPROM
  EEPROM.put(0, totalFlowAccumulated);
  EEPROM.put(4, updateInterval);
  EEPROM.put(8, filterNumber);
  EEPROM.put(12, kFactor);

  Serial.println("Configurações salvas na EEPROM");
}

void displayInitialMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("> Dispense");
  lcd.setCursor(0, 1);
  lcd.print("  Config");
  currentMenu = MENU_INITIAL;
  menuIndex = 0; // Redefine o índice do menu ao exibir o menu inicial
  Serial.println("Menu inicial exibido");
}

void displayWelcome() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("LAZY'Q IND.");
  lcd.setCursor(1, 1);
  lcd.print("AUTO DISPENSER");
  Serial.println("Mensagem de boas-vindas exibida");
}

void handleInitialMenu() {
  // Nenhuma ação necessária aqui, a lógica foi movida para checkButtons()
}

void adjustMenuIndex(int direction, int menuLength) {
  if (currentMenu == MENU_INITIAL) {
    menuIndex += direction;
    if (menuIndex < 0) menuIndex = 0;
    if (menuIndex >= menuLength) menuIndex = menuLength - 1;

    lcd.clear();
    if (menuIndex == 0) {
      lcd.setCursor(0, 0);
      lcd.print("> Dispense");
      lcd.setCursor(0, 1);
      lcd.print("  Config");
    } else if (menuIndex == 1) {
      lcd.setCursor(0, 0);
      lcd.print("  Dispense");
      lcd.setCursor(0, 1);
      lcd.print("> Config");
    }

    Serial.print("Índice do menu ajustado para: ");
    Serial.println(menuIndex);
  } else if (currentMenu == MENU_CONFIG) {
    configIndex += direction;
    if (configIndex < 0) configIndex = 0;
    if (configIndex >= configLength) configIndex = configLength - 1;

    lcd.clear();
    if (configIndex == 0) {
      lcd.setCursor(0, 0);
      lcd.print("> Filter Alert");
      lcd.setCursor(0, 1);
      lcd.print("  Interval");
    } else if (configIndex == 1) {
      lcd.setCursor(0, 0);
      lcd.print("  Filter Alert");
      lcd.setCursor(0, 1);
      lcd.print("> Interval");
    } else if (configIndex == 2) {
      lcd.setCursor(0, 0);
      lcd.print("  Interval");
      lcd.setCursor(0, 1);
      lcd.print("> K Factor");
    } else if (configIndex == 3) {
      lcd.setCursor(0, 0);
      lcd.print("  K Factor");
      lcd.setCursor(0, 1);
      lcd.print("> Reset Accum");
    } else if (configIndex == 4) {
      lcd.setCursor(0, 0);
      lcd.print("  Reset Accum");
      lcd.setCursor(0, 1);
      lcd.print("> Total Accum");
    }

    Serial.print("Índice do menu de configuração ajustado para: ");
    Serial.println(configIndex);
  }
}

void displayConfigMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("> Filter Alert");
  lcd.setCursor(0, 1);
  lcd.print("  Interval");
  currentMenu = MENU_CONFIG;
  configIndex = 0; // Redefine o índice do menu de configuração ao exibir o menu
  Serial.println("Menu de configuração exibido");
}

void handleConfigMenu() {
  // Nenhuma ação necessária aqui, a lógica foi movida para checkButtons()
}

void displayConfigFilter() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Filter Alert:");
  lcd.setCursor(0, 1);
  lcd.print(filterNumber);
  lcd.print(" L");
  Serial.print("Alerta de filtro exibido: ");
  Serial.print(filterNumber);
  Serial.println(" L");
}

void handleConfigFilter() {
  saveConfig(); // Salva a configuração ao modificar o valor
}

void displayConfigInterval() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Interval:");
  lcd.setCursor(0, 1);
  lcd.print(updateInterval);
  lcd.print(" ms");
  Serial.print("Intervalo exibido: ");
  Serial.print(updateInterval);
  Serial.println(" ms");
}

void handleConfigInterval() {
  saveConfig(); // Salva a configuração ao modificar o valor
}

void displayConfigKFactor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("K Factor:");
  lcd.setCursor(0, 1);
  lcd.print(kFactor, 1);
  Serial.print("Fator K exibido: ");
  Serial.println(kFactor);
}

void handleConfigKFactor() {
  saveConfig(); // Salva a configuração ao modificar o valor
}

void displayResetCounter() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reset Accum");
  lcd.setCursor(0, 1);
  lcd.print("OK?");
  Serial.println("Reset do acumulador exibido");
}

void handleResetCounter() {
  totalFlowAccumulated = 0;
  saveConfig(); // Salva a configuração ao modificar o valor
  Serial.println("Acumulador resetado");
}

void displayTotalAccumulated() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Total Accum:");
  lcd.setCursor(0, 1);
  lcd.print(totalFlowAccumulated / 1000.0);
  lcd.print(" L");
  Serial.print("Total acumulado exibido: ");
  Serial.print(totalFlowAccumulated / 1000.0);
  Serial.println(" L");
}

void handleShowTotal() {
  // Apenas exibe a tela de total acumulado
}

void returnToInitialMenu() {
  displayInitialMenu();
  Serial.println("Retorno ao menu inicial");
}
