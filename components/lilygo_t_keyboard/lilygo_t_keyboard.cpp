#include "lilygo_t_keyboard.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lilygo_t_keyboard {

static const char *const TAG = "lilygo_t_keyboard";

void LilygoTKeyboard::setup()
{
    for (byte row : rows_)
    {
        pinMode(row, INPUT);
    }
    for (byte col : cols_)
    {
        pinMode(col, INPUT_PULLUP);
    }
    ESP_LOGCONFIG(TAG, "Setting up LilygoTKeyboard...");
}

void LilygoTKeyboard::dump_config()
{
    ESP_LOGCONFIG(TAG, "LilygoTKeyboard");
}

void LilygoTKeyboard::loop()
{

}

void LilygoTKeyboard::readKey()
{
    for (size_t colIndex = 0; colIndex < cols_.size(); colIndex++)
    {
        byte curCol = cols[colIndex];
        pinMode(curCol, OUTPUT);
        digitalWrite(curCol, LOW);
        for (size_t rowIndex = 0; rowIndex < rows_.size(); rowIndex++)
        {
            byte rowCol = rows[rowIndex];
            pinMode(rowCol, INPUT_PULLUP);
            delay(1);
            bool keyPressed = (digitalRead(rowCol) == LOW);
            int key = colIndex * rows_.size() + rowIndex;
            if (lastPressedKey_[key] != keyPressed)
            {
                lastPressedKey_[key] = keyPressed;
                if (keyPressed) on_press_key(key);
                else            on_release_key(key);
            }
            pinMode(rowCol, INPUT);
        }
        pinMode(curCol, INPUT);
    }
}

void LilygoTKeyboard::on_press_key(int key)
{
    ESP_LOGD(TAG, "press key (%d)", key);
}

void LilygoTKeyboard::on_release_key(int key)
{

}


}  // namespace lilygo_t_keyboard
}  // namespace esphome
