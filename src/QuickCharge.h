#ifndef _QuickCharge_h
#define _QuickCharge_h

#define QC_5V   0x00              // 5V 
#define QC_9V   0x01              // 9V QC2.0, QC3.0A/B
#define QC_12V  0x02              // 12V QC2.0, QC3.0A/B
#define QC_20V  0x03              // 20V QC2.0, QC3.0B
#define QC_VAR  0x04              // QC3.0A 3.6-12V, QC3.0B 3.6-20V

#define QC_CLASS_A  false         // Class A: max 12V
#define QC_CLASS_B  true          // Class B: max 20V

#define QC_NA   0x00    
#define QC_GEN1 0x01              // QC 1.0 - 5V 2A
#define QC_GEN2 0x02              // QC 2.0, QC 3.0

#define GLITCH_CONT_CHANGE  3     // increment/decrement impuls length (ms)
#define GLITCH_VOUT_CHANGE  80    // Delay to allow switching to change (ms)
#define GLITCH_BC_DONE      3000  // 3000ms delay to turn on QC 
#define GLITCH_BC_PLUS      5     // Pairing delay (ms)
#define TEST_PULSE          10    // Test pulse (us)

#define QC_MV_MIN   3600		  // min voltage (mV)
#define QCA_MV_MAX  12000		  // max voltage (mv) QC3.0A
#define QCB_MV_MAX  20000		  // max voltage (mv) QC3.0B

#define SET_HIZ     0x00
#define SET_0MV     0x01
#define SET_600MV   0x02
#define SET_3300MV  0x03

class QuickCharge {
public:
    QuickCharge(uint8_t DP_H, uint8_t DP_L, uint8_t DM_H, uint8_t DM_L) :
    _dp_h(DP_H), _dp_l(DP_L), _dm_h(DM_H), _dm_l(DM_L), _class(QC_CLASS_A) {}

    QuickCharge(uint8_t DP_H, uint8_t DP_L, uint8_t DM_H, uint8_t DM_L, bool CLASS) :
    _dp_h(DP_H), _dp_l(DP_L), _dm_h(DM_H), _dm_l(DM_L), _class(CLASS) {}

    void setClass(bool type) {
        _class = type;
    }

    uint8_t begin() {
        // Are D+ and D- shorted
        _set_dp(SET_3300MV);
		_set_dm(SET_0MV);
		_set_dm(SET_HIZ);
        delayMicroseconds(TEST_PULSE);
        if (digitalRead(_dm_h)) {
            _set_dp(SET_HIZ);
            _set_dm(SET_HIZ);
            _5vOnly = true;
            return QC_NA;
        }

        // No short in the adapter, hopefully there is QC, let's try to turn on QC 
        _set_dp(SET_600MV);             
        delay(GLITCH_BC_DONE);          

        // Check if adapter disconnected D+ and D- internally
        _set_dp(SET_3300MV);
        delayMicroseconds(TEST_PULSE);
        if (!digitalRead(_dm_h)) {
            _set_dp(SET_600MV);
            pinMode(_dm_h, OUTPUT);
            pinMode(_dm_l, OUTPUT);
            _5vOnly = false;
            return QC_GEN2;
        }

        // Normal 5V 2A adapter
        _set_dp(SET_HIZ);
        _set_dm(SET_HIZ);
        _5vOnly = true;
        return QC_GEN1;
    }


    void setMode(uint8_t mode) {
        /*
        | (D+) | (D-) |   Mode   |
        | 0.6V |  0V  |   5V     |
        | 3.3V | 0.6V |   9V     |
        | 0.6V | 0.6V |   12V    |
        | 3.3V | 3.3V |   20V    |
        | 0.6V | 3.3V | VARIABLE |
        */
        switch (mode) {
        case QC_5V:
            _set_dp(SET_600MV);
            _set_dm(SET_0MV);
            _mv = 5000;
            break;
        case QC_9V:
            if (_5vOnly) break;
            _set_dp(SET_3300MV);
            _set_dm(SET_600MV);
            _mv = 9000;
            break;
        case QC_12V:
            if (_5vOnly) break;
            _set_dp(SET_600MV);
            _set_dm(SET_600MV);
            _mv = 12000;
            break;
        case QC_20V:
            if (_5vOnly || !_class) break;
            _set_dp(SET_3300MV);
            _set_dm(SET_3300MV);
            _mv = 20000;
            break;
        case QC_VAR:
            if (_5vOnly) break;
            _set_dp(SET_600MV);
            _set_dm(SET_3300MV);
            break;
        }
        delay(GLITCH_VOUT_CHANGE);
    }

    void set(int16_t mv) {
        if(_5vOnly) return;
        int16_t error = constrain(mv, QC_MV_MIN, _class ? QCB_MV_MAX : QCA_MV_MAX) - _mv; 
        bool dir = true;                                
        if (error < 0) error = -error, dir = false;     
        for (uint8_t i = 0; i < error / 200; i++) {     
            if (dir) inc();                             
            else dec();                                 
        }
    }

    void inc() {
        if(_5vOnly) return;
        _set_dp(SET_3300MV);
        delay(GLITCH_CONT_CHANGE);
        _set_dp(SET_600MV);
        delay(GLITCH_CONT_CHANGE);
        _mv = constrain(_mv + 200, QC_MV_MIN, _class ? QCB_MV_MAX : QCA_MV_MAX);
    }

    void dec() {
        if(_5vOnly) return;
        _set_dm(SET_600MV);
        delay(GLITCH_CONT_CHANGE);
        _set_dm(SET_3300MV);
        delay(GLITCH_CONT_CHANGE);
        _mv = constrain(_mv - 200, QC_MV_MIN, _class ? QCB_MV_MAX : QCA_MV_MAX);
    }
    
    int16_t voltage() {
        return _mv;
    }

private:
    void _set_dp(uint8_t state) {
      switch (state) {
        case SET_HIZ:    
          pinMode(_dp_h, INPUT);
          pinMode(_dp_l, INPUT); 
          break;
        case SET_0MV:    
          pinMode(_dp_h, OUTPUT);
          pinMode(_dp_l, OUTPUT);
          digitalWrite(_dp_h, LOW);
          digitalWrite(_dp_l, LOW);
          break; 
        case SET_600MV:
          pinMode(_dp_h, OUTPUT);
          pinMode(_dp_l, OUTPUT);
          digitalWrite(_dp_h, HIGH);
          digitalWrite(_dp_l, LOW);
          break; 
        case SET_3300MV: 
          pinMode(_dp_h, OUTPUT);
          pinMode(_dp_l, OUTPUT);
          digitalWrite(_dp_h, HIGH);
          digitalWrite(_dp_l, HIGH); 
          break; 
      }
    }

    void _set_dm(uint8_t state) {
      switch (state) {
        case SET_HIZ:    
          pinMode(_dm_h, INPUT);
          pinMode(_dm_l, INPUT);
          break;               
        case SET_0MV:    
          pinMode(_dm_h, OUTPUT);
          pinMode(_dm_l, OUTPUT);
          digitalWrite(_dm_h, LOW);
          digitalWrite(_dm_l, LOW); 
          break; 
        case SET_600MV:  
          pinMode(_dm_h, OUTPUT);
          pinMode(_dm_l, OUTPUT);
          digitalWrite(_dm_h, HIGH);
          digitalWrite(_dm_l, LOW);
          break; 
        case SET_3300MV: 
          pinMode(_dm_h, OUTPUT);
          pinMode(_dm_l, OUTPUT);
          digitalWrite(_dm_h, HIGH);
          digitalWrite(_dm_l, HIGH);
          break; 
      }
    }
    
    int16_t _mv = 5000;
    
    const uint8_t _dp_h = 0;
    const uint8_t _dp_l = 0;
    const uint8_t _dm_h = 0;
    const uint8_t _dm_l = 0;
    bool _5vOnly = true;
    bool _class = false;
};
#endif