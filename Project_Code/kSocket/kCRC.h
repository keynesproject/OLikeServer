#pragma once 

//����p��CRC�A�t�׸��C�A�����γ̤p�����s�Ŷ�;//
unsigned int CalCrcBit( unsigned char *ptr, unsigned char len );


//���r�`�p��CRC�A�t�׸��֡A�����θ��j�����s;//
unsigned int CalCrcByte(unsigned char *ptr, unsigned char len);


//���b�r�`�p��A�O�e��̪����šA���A�X8bit�p���s�������������;//
unsigned int CalCrcHalfByte(unsigned char *ptr, unsigned char len);

