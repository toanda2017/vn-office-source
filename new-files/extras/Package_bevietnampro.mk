$(eval $(call gb_Package_Package,extras_bevietnampro,$(SRCDIR)/extras/source/truetype/bevietnampro))

$(eval $(call gb_Package_add_files,extras_bevietnampro,$(LIBO_SHARE_FOLDER)/fonts/truetype,\
    BeVietnamPro-Black.ttf \
    BeVietnamPro-BlackItalic.ttf \
    BeVietnamPro-Bold.ttf \
    BeVietnamPro-BoldItalic.ttf \
    BeVietnamPro-ExtraBold.ttf \
    BeVietnamPro-ExtraBoldItalic.ttf \
    BeVietnamPro-ExtraLight.ttf \
    BeVietnamPro-ExtraLightItalic.ttf \
    BeVietnamPro-Italic.ttf \
    BeVietnamPro-Light.ttf \
    BeVietnamPro-LightItalic.ttf \
    BeVietnamPro-Medium.ttf \
    BeVietnamPro-MediumItalic.ttf \
    BeVietnamPro-Regular.ttf \
    BeVietnamPro-SemiBold.ttf \
    BeVietnamPro-SemiBoldItalic.ttf \
    BeVietnamPro-Thin.ttf \
    BeVietnamPro-ThinItalic.ttf \
))
