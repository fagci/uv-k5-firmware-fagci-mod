sed -i 's/\bBK4819_GPIO5_PIN1\b/BK4819_GPIO1_PIN29_PA_ENABLE/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bBK4819_GPIO6_PIN2\b/BK4819_GPIO0_PIN28_RX_ENABLE/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bBK4819_GPIO2_PIN30\b/BK4819_GPIO4_PIN32_VHF_LNA/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bBK4819_GPIO3_PIN31\b/BK4819_GPIO3_PIN31_UHF_LNA/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bBK4819_GPIO1_PIN29_RED\b/BK4819_GPIO5_PIN1_RED/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bBK4819_Conditional_RX_TurnOn_and_GPIO6_Enable\b/BK4819_EnableRX/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bBK4819_PickRXFilterPathBasedOnFrequency\b/BK4819_SelectFilter/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bVFO_CONFIGURE_0\b/VFO_CONFIGURE_NONE/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bVFO_CONFIGURE_1\b/VFO_CONFIGURE/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bRX_CHANNEL\b/RX_VFO/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bTX_CHANNEL\b/TX_VFO/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/bDoScan/bEnable/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bRADIO_SetTxParameters\b/RADIO_enableTX/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bFREQUENCY_DEVIATION_SETTING\b/OFFSET_DIR/' ./**/*.{c,h} ./*.{c,h}
sed -i 's/\bgScreenToDisplay ([!]\=) DISPLAY_SCANNER\b/gAppToDisplay \1 APP_SCANNER/' ./**/*.{c,h} ./*.{c,h}
