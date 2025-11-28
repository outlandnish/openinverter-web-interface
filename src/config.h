#ifndef CONFIG_H
#define CONFIG_H

#define EEPROM_VERSION 3
typedef struct {
    int version;
    int canRXPin;
    int canTXPin;
    int canEnablePin;
    int canSpeed;
} EEPROMSettings;


class Config
{
  public:
    Config();
    void load();
    int getCanRXPin();
    void setCanRXPin(int pin);

    int getCanTXPin();
    void setCanTXPin(int pin);


    int getCanEnablePin();
    void setCanEnablePin(int pin);

    int getCanSpeed();
    void setCanSpeed(int speed);

    void saveSettings();
  private:
    EEPROMSettings settings;

};
#endif