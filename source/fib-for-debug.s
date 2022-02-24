# フィボナッチ数列を計算し、値を順にメモリに格納

start:
      sw %x0, 0(%x0)    # f(0) = 0
      addi %x1, %x0, 1
      sw %x1, 1(%x0)    # f(1) = 1
      addi %x1, %x0, 0  # カウンタ用変数
      addi %x2, %x0, 10 # f(n+3)まで計算

loop: ! # ラベルにブレークポイントを付ける場合は名前を指定しない
      blt %x2, %x1, exit
      lw %x3, 0(%x1)
      lw %x4, 1(%x1) ! test  # ラベルのない行にブレークポイントを付ける場合は名前を指定
      add %x5, %x3, %x4 # f(n+2) = f(n+1) + f(n)
      sw %x5, 2(%x1)
      addi %x1, %x1, 1
      jal %x0, loop

exit:
      jal %x0, exit
