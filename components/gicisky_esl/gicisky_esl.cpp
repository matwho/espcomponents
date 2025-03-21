#include "gicisky_esl.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <numeric>
#include <limits>

#ifdef USE_ESP32
namespace esphome {
namespace gicisky_esl {

static const char *const TAG = "gicisky_esl";

void GiciskyESL::dump_config()
{
    LOG_DISPLAY("", "gicisky_esl", this);
    ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d", this->width_, this->height_);
    ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str().c_str());
    ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
    ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->cmd_uuid_.to_string().c_str());
    LOG_UPDATE_INTERVAL(this);
}

void GiciskyESL::update()
{
    // this->do_update_();
    // this->display_();
}

void GiciskyESL::setup()
{
    image_buffer_.resize(this->width_ * this->height_);
    image_packet_.resize(this->width_ * this->height_ / 4);
    std::fill(image_buffer_.begin(), image_buffer_.end(), Color(0, 0, 0));
    if (this->version_) this->version_->publish_state(VERSION);
    if (this->bt_connected_) this->bt_connected_->publish_state(false);
    timer_ = get_time();
    ESP_LOGI(TAG, "Initaialize.");
}


void GiciskyESL::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event)
    {
        case ESP_GATTC_OPEN_EVT: 
        {
            if (param->open.status == ESP_GATT_OK) 
            {
                espbt::global_esp32_ble_tracker->stop_scan();
                this->do_update_();
                this->display_();
                connected_ = true;
                if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
                ESP_LOGI(TAG, "[%s] Connected successfully!", this->parent_->address_str().c_str());
                break;
            }
            break;
        }
        case ESP_GATTC_CLOSE_EVT: 
        {
            this->status_set_warning();
            connected_ = false;
            if (this->bt_connected_) this->bt_connected_->publish_state(connected_);
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT: 
        {
            this->handle = 0;
            auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->cmd_uuid_);
            if (chr == nullptr) 
            {
                this->status_set_warning();
                ESP_LOGD(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(), this->cmd_uuid_.to_string().c_str());
                break;
            }
            else
            {
                ESP_LOGI(TAG, "Characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(), this->cmd_uuid_.to_string().c_str());
            }
            this->handle = chr->handle;
            auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                            this->parent()->get_remote_bda(), chr->handle);
            if (status) {
                ESP_LOGD(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
            }
            break;
        }
        case ESP_GATTC_READ_CHAR_EVT: 
        {
            if (param->read.handle == this->handle) 
            {
                if (param->read.status != ESP_GATT_OK) {
                    ESP_LOGD(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
                    break;
                }
                this->status_clear_warning();
                this->parse_data(param->read.value, param->read.value_len);
            }
            break;
        }
        case ESP_GATTC_NOTIFY_EVT: 
        {
            if (param->notify.handle != this->handle)
                break;
            ESP_LOGD(TAG, "[%s] ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x", this->parent_->address_str().c_str(),
                    param->notify.handle, param->notify.value[0]);
            this->parse_data(param->notify.value, param->notify.value_len);
            break;
        }
        case ESP_GATTC_REG_FOR_NOTIFY_EVT: 
        {
            if (param->reg_for_notify.status == ESP_GATT_OK && param->reg_for_notify.handle == this->handle)
            {
                //this->node_state = espbt::ClientState::ESTABLISHED;
                send_cmd(0x01);
            }
                
            break;
        }
        default:
        break;
    }
}

void GiciskyESL::parse_data(uint8_t *data, uint16_t len)
{
    if (len < 1) return;
    ESP_LOGD(TAG, "Recive-> %s", to_hex_string(data, len).c_str());
    uint32_t part = 0;
    switch(data[0])
    {
    case 0x01:
        if (len < 3) break;
        if (data[1] != 0xf4) break;
        if (data[2] != 0x00) break;
        send_cmd(0x02);
        break;
    case 0x02:
        send_cmd(0x03);
        break;
    case 0x05:
        if (len < 6) break;
        if (data[1] == 0x08)
        {
            //End
            this->node_state = espbt::ClientState::ESTABLISHED;
            this->parent()->disconnect();
            espbt::global_esp32_ble_tracker->start_scan();
        }
        else if (data[1] == 0x00)
        {
            part = (data[5] << 24) | (data[4] << 16) | (data[3] << 8) | data[2];
            send_img(part);
        }
        break;
    }
}

void GiciskyESL::send_cmd(uint8_t cmd)
{
    std::vector<uint8_t> packet = {cmd};
    if (cmd == 0x02)
    {
        uint32_t size = get_buffer_length_() / 4;
        packet.push_back((uint8_t)(size & 0xff));
        packet.push_back((uint8_t)((size >> 8) & 0xff));
        packet.push_back((uint8_t)((size >> 16) & 0xff));
        packet.push_back((uint8_t)((size >> 24) & 0xff));
        packet.push_back((uint8_t)0x00);
        packet.push_back((uint8_t)0x00);
        packet.push_back((uint8_t)0x00);
    }
    ESP_LOGD(TAG, "Send CMD -> %s", to_hex_string(packet).c_str());
    delay(100);
    this->write_cmd(packet);
}

void GiciskyESL::send_img(uint32_t part)
{
    uint32_t total_size = get_buffer_length_() / 4;
    uint32_t len = total_size - (part  * 240);
    if (len > 240) len = 240;
    std::vector<uint8_t> packet;
    packet.push_back((uint8_t)(part & 0xff));
    packet.push_back((uint8_t)((part >> 8) & 0xff));
    packet.push_back((uint8_t)((part >> 16) & 0xff));
    packet.push_back((uint8_t)((part >> 24) & 0xff));
    for (uint32_t i = 0; i < len; i++) 
    {
        uint32_t idx = (part * 240) + (i);
        packet.push_back(image_packet_[idx]);
    }
    write_img(packet);
}

unsigned long GiciskyESL::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long GiciskyESL::get_time()
{
    return millis();
}

void GiciskyESL::shift_image()
{
    int32_t offset = 0;
    uint8_t currentByte = 0;
    uint8_t currentByteRed = 0;
    int bitPosition = 7;
    std::vector<uint8_t> byteData;
    std::vector<uint8_t> byteDataRed;
    for (int x = 0; x < this->width_; x++)
    {
        for (int y = 0; y < this->height_; y++)
        {
            uint32_t pos = (y * width_) + x;
            Color color = image_buffer_[pos];

            if (color != Color(0, 0, 0)) {
                currentByte |= (1 << bitPosition);
            }
            if (color == Color(255, 0, 0)) {
                currentByteRed |= (1 << bitPosition);
            }
            bitPosition--;
            if (bitPosition < 0) {
                byteData.push_back(currentByte);
                byteDataRed.push_back(currentByteRed);
                currentByte = 0;
                currentByteRed = 0;
                bitPosition = 7;
            }
        }
    }
    if (bitPosition != 7) {
        byteData.push_back(currentByte);
        byteDataRed.push_back(currentByteRed);
    }
    uint32_t idx = 0;
    for (uint8_t byte : byteData)
    {
        image_packet_[idx++] = byte;
    }
    for (uint8_t byte : byteDataRed)
    {
        image_packet_[idx++] = byte;
    }

}

void GiciskyESL::display_()
{
    shift_image();
}

void HOT GiciskyESL::draw_absolute_pixel_internal(int x, int y, Color color)
{
    if (x < 0) return;
    if (x >= this->get_width_internal()) return;
    if (y >= this->get_height_internal() || y < 0) return;
    uint32_t pos = (y * width_) + x;
    image_buffer_[pos] = color;
    //ESP_LOGI(TAG, "Color %d %d r%d g%d b%d", x, y, color.r, color.g, color.b);
}

bool GiciskyESL::write_cmd(std::vector<uint8_t> &data)
{
    // if (this->node_state != espbt::ClientState::ESTABLISHED)
    // {
    //     ESP_LOGD(TAG, "[%s] Not connected to BLE client.  State update can not be written.", this->cmd_uuid_.to_string().c_str());
    //     return false;
    // }
    auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->cmd_uuid_);
    if (chr == nullptr)
    {
        ESP_LOGD(TAG, "[%s] Characteristic not found.  State update can not be written.", this->cmd_uuid_.to_string().c_str());
        return false;
    }
    chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_RSP);
    ESP_LOGI(TAG, "Write array-> %s", to_hex_string(data).c_str());
    return true;
}


bool GiciskyESL::write_img(std::vector<uint8_t> &data)
{
    // if (this->node_state != espbt::ClientState::ESTABLISHED)
    // {
    //     ESP_LOGD(TAG, "[%s] Not connected to BLE client.  State update can not be written.", this->img_uuid_.to_string().c_str());
    //     return false;
    // }
    auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->img_uuid_);
    if (chr == nullptr)
    {
        ESP_LOGD(TAG, "[%s] Characteristic not found.  State update can not be written.", this->img_uuid_.to_string().c_str());
        return false;
    }
    chr->write_value(&data[0], data.size(), ESP_GATT_WRITE_TYPE_RSP);
    ESP_LOGI(TAG, "Write array-> %s", to_hex_string(data).c_str());
    return true;
}

std::string GiciskyESL::to_hex_string(const std::vector<unsigned char> &data)
{
    char buf[20];
    std::string res;
    for (uint16_t i = 0; i < data.size(); i++)
    {
        sprintf(buf, "0x%02X ", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", data.size());
    res += buf;
    return res;
}

std::string GiciskyESL::to_hex_string(const uint8_t* data, const uint16_t len)
{
    char buf[20];
    std::string res;
    for (uint16_t i = 0; i < len; i++)
    {
        sprintf(buf, "%02X", data[i]);
        res += buf;
    }
    sprintf(buf, "(%d)", len);
    res += buf;
    return res;
}

} // namespace gicisky_esl
}  // namespace esphome
#endif