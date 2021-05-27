#ifndef __css_descramble_h_
#define __css_descramble_h_

struct playkey {
	int offset;
	unsigned char key[5];
};

extern int css_decrypttitlekey(unsigned char *tkey, unsigned char *dkey, struct playkey **pkey);
extern void css_descramble(unsigned char *sec,unsigned char *key);

#endif
