#pragma once 

//按位計算CRC，速度較慢，但佔用最小的內存空間;//
unsigned int CalCrcBit( unsigned char *ptr, unsigned char len );


//按字節計算CRC，速度較快，但佔用較大的內存;//
unsigned int CalCrcByte(unsigned char *ptr, unsigned char len);


//按半字節計算，是前兩者的均衡，較適合8bit小內存的單片機的應用;//
unsigned int CalCrcHalfByte(unsigned char *ptr, unsigned char len);

