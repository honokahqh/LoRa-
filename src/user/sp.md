发送方
1.init 
2.周期调用Protocol_SP_Send
3.有数据需要载入时调用Protocol_SP_Load_Data 
4.周期调用
接收方
1.判断是否为本轮最后一包
	1.1 rec_end = 1
	1.2 判断CRC，清标志位，载入缓存