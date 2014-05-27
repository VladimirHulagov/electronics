struct radio_frame_s 
{
	uint8_t* ptr;
	uint8_t length;
	uint8_t maxlength;
};

extern	int hal_radio_init();
extern	int hal_radio_receive_frame( struct radio_frame_s* precvframe );
extern	int hal_radio_send_frame( const struct radio_frame_s* pframe );
