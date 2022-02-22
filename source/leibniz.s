# ライプニッツ級数により円周率を計算し、値をメモリの0番地に格納
# ※ライプニッツ級数: \sum_{i=0}^{\infty} (-1)^i / (2i+1) = \pi / 4

start:
	lui %x1, 0x3f800		# 1.0の上位20bit
	addi %x1, %x1, 0		# 1.0の下位12bit
	fmv.i.f %f1, %x1		# f1: 1.0
	lui %x1, 0x40000		# 2.0の上位20bit
	addi %x1, %x1, 0		# 2.0の下位12bit
	fmv.i.f %f2, %x1		# f2: 2.0
	fadd %f3, %f0, %f0		# f3: acc=0
	fadd %f4, %f1, %f0		# f4: n (1,3,5,....)
	lui %x1, 1000			# x1: N: ループの回数
	addi %x2, %x0, 0		# x2: i (ステップのカウント)
	addi %x3, %x0, 1		# x3: j (偶奇判定)

loop:
	blt %x1, %x2, break		# while N > i
	fdiv %f5, %f1, %f4		# f5: 1/b
	blt %x0, %x3, plus		# if j > 0 then goto +
	fsub %f3, %f3, %f5		# else a -= 1/b
	jal %x0, cont

plus:
	fadd %f3, %f3, %f5		# then acc += 1/b

cont:
	addi %x2, %x2, 1		# i += 1
	sub %x3, %x0, %x3		# j := -j
	fadd %f4, %f4, %f2		# b += 2.0
	jal %x0, loop

break:
	fmul %f3, %f3, %f2
	fmul %f3, %f3, %f2		# acc ×= 4
	fsw %f3, 0(%x0)			# acc = \pi
	flw %f3, 0(%x0)

exit:
	jal %x0, exit
