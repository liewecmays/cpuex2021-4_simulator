# 値をサーバから受け取ってそのまま返す
# 通信のテスト用(lre, lrd, ltf, std)

check_empty:
    lre %x3
    blt %x0, %x3, check_empty
    lrd %x4

check_full:
    ltf %x3
    blt %x0, %x3, check_full
    std %x4
    jal %x0, check_empty
