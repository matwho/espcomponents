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
    this->do_update_();
    this->display_();
}

void GiciskyESL::setup()
{
    image_buffer_.resize(this->width_ * this->height_);
    clear_display_buffer();
    if (this->version_) this->version_->publish_state(VERSION);
    if (this->bt_connected_) this->bt_connected_->publish_state(false);
    if (this->update_)
    {
        this->update_->add_on_state_callback(std::bind(&GiciskyESL::update_callback, this, std::placeholders::_1));
        this->update_->turn_off();
    }
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
            if (this->update_)
            {
                this->node_state = espbt::ClientState::ESTABLISHED;
                this->update_->turn_off();
            }
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
        uint32_t size = get_buffer_length_();
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
    uint32_t total_size = get_buffer_length_();
    uint32_t len = total_size - (part  * 240);
    if (len > 240) len = 240;
    std::vector<uint8_t> packet;
    packet.push_back((uint8_t)(part & 0xff));
    packet.push_back((uint8_t)((part >> 8) & 0xff));
    packet.push_back((uint8_t)((part >> 16) & 0xff));
    packet.push_back((uint8_t)((part >> 24) & 0xff));
    for (uint32_t i = 0; i < len; i++) 
    {
        uint32_t idx = (part * 240) + i;
        Color color = image_buffer_[idx];
        packet.push_back(color.r);
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

void GiciskyESL::clear_display_buffer()
{
    display_list_.clear();
    this->x_high_ = 0;
    this->y_high_ = 0;
}

void GiciskyESL::shift_image()
{
    int32_t offset = 0;
    for (int x = 0; x < this->width_; x++)
    {
        for (int y = 0; y < this->height_; y++)
        {
            uint32_t pos = (y * width_) + x;
            image_buffer_[pos] = get_display_color(x + offset, y);
        }
    }
}

void GiciskyESL::display_()
{
    if (!connected_) return;
    shift_image();
    clear_display_buffer();
    if (image_buffer_.size() == old_image_buffer_.size())
    {
        if (std::equal(image_buffer_.begin(), image_buffer_.end(), old_image_buffer_.begin())) return;
    }
    ESP_LOGD(TAG, "Update Display");
    //old_image_buffer_ = image_buffer_;
    //send_cmd(0x01);
}

void HOT GiciskyESL::draw_absolute_pixel_internal(int x, int y, Color color)
{
    if (x < 0) return;
    //if (x >= this->get_width_internal()) return;
    if (y >= this->get_height_internal() || y < 0) return;
    add_color_point(ColorPoint(x, y, color));
    //ESP_LOGI(TAG, "Color %d %d r%d g%d b%d", x, y, color.r, color.g, color.b);
    if (this->x_high_ < x) this->x_high_ = x;
    if (this->y_high_ < y) this->y_high_ = y;
}

void GiciskyESL::add_color_point(ColorPoint point)
{
    if (point.color == background_color_) return;
    for (int i = 0; i < display_list_.size(); i++)
    {
        if (display_list_[i].x == point.x && display_list_[i].y == point.y)
        {
            display_list_[i].color = point.color;
            return;
        }
    }
    display_list_.push_back(point);
}

Color GiciskyESL::get_display_color(int x, int y)
{
    for (ColorPoint point : display_list_)
    {
        if (point.x == x && point.y == y)
        {
            //ESP_LOGI(TAG, "Color %d %d r%d g%d b%d", point.x, point.y, point.color.r, point.color.g, point.color.b);
            return point.color;
        }
    }
    return background_color_;
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

void GiciskyESL::update_callback(bool state)
{
    if (state)
    {
        if (!connected_)
        {
            espbt::global_esp32_ble_tracker->stop_scan();
            this->parent()->connect();
        }
    }
    else
    {
        if (connected_)
        {
            this->parent()->disconnect();
            //espbt::global_esp32_ble_tracker->start_scan();
        }

    }
}


} // namespace gicisky_esl
}  // namespace esphome
#endif