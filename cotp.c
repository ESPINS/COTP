
#include "cotp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/*
	Allocates a new OTPData struct. Initializes its values.

	Returns
			A pointer to the new struct
		error, 0
*/
OTPData* otp_new(const char* base32_secret, size_t bits, COTP_ALGO algo, const char* digest, size_t digits) {
	OTPData* data = malloc(sizeof(OTPData));
	if(data == 0)
		return 0;
	data->digits = digits ? digits : 6;
	
	data->base32_secret = &base32_secret[0];
	data->digest = &digest[0];
	data->algo = algo;
	data->bits = bits;
	
	data->method = OTP;
	return data;
}

/*
	Allocates a new OTPData struct. Extends off of otp_new. Initializes its values furhter.
	
	Returns
			A pointer to the new struct
		error, 0
*/
OTPData* totp_new(const char* base32_secret, size_t bits, COTP_ALGO algo, const char* digest, size_t digits, size_t interval) {
	OTPData* data = otp_new(base32_secret, bits, algo, digest, digits);
	data->interval = interval;
	data->method = TOTP;
	return data;
}

/*
	Allocates a new OTPData struct. Extends off of otp_new. Initializes its values furhter.
	
	Returns
			A pointer to the new struct
		error, 0
*/
OTPData* hotp_new(const char* base32_secret, size_t bits, COTP_ALGO algo, const char* digest, size_t digits) {
	OTPData* data = otp_new(base32_secret, bits, algo, digest, digits);
	data->method = HOTP;
	return data;
}


/*
	Frees data allocated by *otp_new() calls.
*/
void otp_free(OTPData* data) {
	free(data);
}

/*
	un-base32's a base32 string using data as instructions, size as validation,
	and out_str as output.
	
	Returns
			1 success
		if out_str != 0, writes generated base10 as string to out_str
		error, 0
*/
int otp_byte_secret(OTPData* data, size_t size, char* out_str) {
	if(out_str == NULL || size % 8 != 0)
		return 0;
	int n = 5;
	for (size_t i=0; ; i++) {
		n = -1;
		out_str[i*5] = 0;
		for (int block=0; block<8; block++) {
			int offset = (3 - (5*block) % 8);
			int octet = (block*5) / 8;
			
			unsigned int c = data->base32_secret[i*8+block];
			if (c >= 'A' && c <= 'Z')
				n = c - 'A';
			if (c >= '2' && c <= '7')
				n = 26 + c - '2';
			if (n < 0) {
				n = octet;
				break;
			}
			out_str[i*5+octet] |= -offset > 0 ? n >> -offset : n << offset;
			if (offset < 0)
				out_str[i*5+octet+1] = -(8 + offset) > 0 ? n >> -(8 + offset) : n << (8 + offset);
		}
		if(n < 5)
			break;
	}
	return 1;
}

/*
	Converts an integer into 8 byte array, where the first 4 bytes are \0's.
	
	Returns
			1 success
			if out_str != 0, writes generated byte array as string to out_str
		error, 0
*/
int otp_int_to_bytestring(int integer, char* out_str) {
	if(out_str == NULL)
		return 0;
	out_str[4] = integer >> 24; // I don't like this method of breaking down the bytes
	out_str[4+1] = integer >> 16;
	out_str[4+2] = integer >> 8;
	out_str[4+3] = integer;
	return 1;
}

/*
	Generates a valid base32 number given len as size, chars as a charset,
	and out_str as output. out_str's size should be precomputed and
	null-terminated.
	
	Returns
			1 on success
			if out_str != 0, writes generated base32 as string to out_str
		error, 0

*/
int otp_random_base32(size_t len, const char* chars, char* out_str) {
	if(chars == NULL || out_str == NULL)
		return 0;
	len = len > 0 ? len : 16;
	for (size_t i=0; i<len; i++)
		out_str[i] = chars[rand()%32];
	return 1;
}


/*
	Compares using data as instructions, key as comparison data,
	increment as offset of timeblock generated from for_time, and for_time as
	time in seconds. Converts key to string. Converts
	count to string to do a string comparison between key as string.
	
	Returns
			1 success, 0 no full comparison made
		error, 0
*/
int totp_compares(OTPData* data, char* key, size_t increment, unsigned int for_time) {
	char* time_str = calloc(8, sizeof(char));
	if(totp_at(data, for_time, increment, time_str) == 0) {
		free(time_str);
		return 0;
	}
	for (size_t i=0; i<8; i++) {
		if(key[i] != time_str[i]) {
			free(time_str);
			return 0;
		}
	}
	free(time_str);
	return 1;
}

/*
	Compares using data as instructions, key as comparison data,
	increment as offset of timeblock generated from for_time, and for_time as
	time in seconds. Converts key to string.
	
	Returns
			1 success, 0 no full comparison made
		error, 0
*/
int totp_comparei(OTPData* data, int key, size_t increment, unsigned int for_time) {
	char* key_str = calloc(8, sizeof(char));
	sprintf(key_str, "%d", key);
	int status = totp_compares(data, key_str, increment, for_time);
	free(key_str);
	return status;
}

/*
	Generates a totp given data as instructions, for_time as time in seconds,
	counter_offset as an offset to generated timeblock from for_time,
	and out_str as output. out_str's size should be precomputed and
	null-terminated. If out_str is null, nothing is wrote to it.
	
	Returns
			> 0 TOTP for the current input based off struct OTPDATA
		if out_str != 0, writes generated TOTP as string to out_str
		error, 0
*/
int totp_at(OTPData* data, unsigned int for_time, size_t counter_offset, char* out_str) {
	return otp_generate(data, totp_timecode(data, for_time) + counter_offset, out_str);
}

/*
	Generates a totp given data as instructions, and out_str as output.
	out_str's size should be precomputed and null-terminated.
	If out_str is null, nothing is wrote to it. Uses time(NULL) for
	the current time to be generated into a timeblock based on
	data->interval.
	
	Returns
			> 0 TOTP for the current input based off struct OTPDATA
		if out_str != 0, writes generated TOTP as string to out_str
		error, 0
*/
int totp_now(OTPData* data, char* out_str) {
	return otp_generate(data, totp_timecode(data, time(NULL)), out_str);
}

/*
	Compares using data as instructions, key as comparison data, for_time
	as time in seconds, and valid_window as offset to for_time timeblock.
	
	Returns
			1 success, 0 no full comparison made
		if valid_window < 0, 0
		error, 0
*/
int totp_verify(OTPData* data, int key, unsigned int for_time, int valid_window) {
	if(valid_window < 0)
		return 0;
	if(valid_window > 0) {
		for (int i=-valid_window; i<valid_window; i++) {
			const int cmp = totp_comparei(data, key, i, for_time);
			if(cmp == 1)
				return cmp;
		}
		return 0;
	}
	return totp_comparei(data, key, 0, for_time);
}

/*
	Calculates using data as instructions, for time as time in seconds,
	and valid_window as offset to for_time timeblock a time where a key
	is alive for based off a certain time point in seconds.
	
	Returns
			time in seconds relative to for_time, using data->interval
*/
unsigned int totp_valid_until(OTPData* data, unsigned int for_time, size_t valid_window) {
	return for_time + (data->interval * valid_window);
}

/*
	Generates a timeblock using data as instructions, and a time in seconds.
	
	Timeblocks are the amount of intervals in a given time. For example,
	if 1m seconds has passed, the amount 1m/30 would give the amount of
	30 seconds in 1m seconds. As an integer, there is no needless decimals.
	
	Returns
			timeblock given for_time, using data->interval
		error, 0
*/
int totp_timecode(OTPData* data, unsigned int for_time) {
	if(data->interval <= 0)
		return 0;
	return for_time/data->interval;
}



/*
	Compares using data as instructions, key as comparison data,
	and counter as comparison data. Converts key to string. Converts
	count to string to do a string comparison between key as string.
	
	Returns
			1 success, 0 no full comparison made
		error, 0
*/
int hotp_comparei(OTPData* data, int key, size_t counter) {
	char* key_str = calloc(8, sizeof(char));
	if(key_str == 0)
		return 0;
	sprintf(key_str, "%d", key);
	int status = hotp_compares(data, key_str, counter);
	free(key_str);
	return status;
}

/*
	Compares using data as instructions, key as comparison data,
	and counter as comparison data. Converts count to string
	to do a string comparison between key.
	
	Returns
			1 success, 0 no full comparison made
		error, 0
*/
int hotp_compares(OTPData* data, char* key, size_t counter) {
	char* cnt_str = calloc(8, sizeof(char));
	sprintf(cnt_str, "%Iu", counter);
	if(cnt_str == 0)
		return 0;
	if(hotp_at(data, counter, cnt_str) == 0) {
		free(cnt_str);
		return 0;
	}
	for (size_t i=0; i<8; i++) {
		if(key[i] != cnt_str[i]) {
			free(cnt_str);
			return 0;
		}
	}
	free(cnt_str);
	return 1;
}

/*
	Generates a hotp given data as instructions, count as data,
	and out_str as output. out_str's size should be precomputed and
	null-terminated. If out_str is null, nothing is wrote to it.
	
	Returns
			> 0 HOTP for the current input based off struct OTPDATA
		if out_str != 0, writes generated HOTP as string to out_str
		error, 0
*/
int hotp_at(OTPData* data, size_t counter, char* out_str) {
	return otp_generate(data, counter, out_str);
}


/*
	Needless function, for library fluency. Compares using data as instructions,
	key as comparison data, and counter as comparison data.
	
	Returns
			1 if hotp_comparei is successful
			0 if hotp_comparei is unsuccessful
		error, 0
*/
int hotp_verify(OTPData* data, int key, size_t counter) {
	return hotp_comparei(data, key, counter);
}

/*
	Generates a one time password given data as instructions, input as data,
	and out_str as output. Input should be > 0. out_str's size should be
	precomputed and null-terminated. If out_str is null, nothing is wrote
	to it.
	
	Returns
			> 0 OTP for the current input based off struct OTPData
		if out_str != 0, writes generated OTP as string to out_str
		error, 0 oom or failed to generate anything based off data in OTPData
	
	// TODO: check out making input unsigned, and avoid having to
	//   do checks on a string rather than a digit. Original implementation
	//   of github.com/pyotp/pyotp does string comparison and a very expensive
	//   string checking. It isn't totally necessary to do depending on the
	//   low security requirements - as long as the login checks are limited
	//   absolutely no hacking is possible. Hacking SHA is impossible anyways
	//   especially with this method, as there is no way to tell if a SHA
	//   key generation is valid for the given input or just a temp collision.
	//   Should make new functions for this though.
*/
int otp_generate(OTPData* data, int input, char* out_str) {
	if(input < 0) return 0;
	
	int code = 0;
	
	char* byte_string = 0;
	char* byte_secret = 0;
	char* hmac = 0;
	
	// de-BASE32 sizes
	size_t secret_len = strlen(data->base32_secret);
	size_t desired_secret_len = (secret_len / 8) * 5;
	
	// de-SHA size
	int bit_size = data->bits/8;
	
	// space for OTP byte secret de-BASE32
	// space for converting input to byte string
	// space for de-SHA
	// de-BASE32, convert to byte string, de-SHA
	byte_string = calloc(8+1, sizeof(char));
	byte_secret = calloc(desired_secret_len+1, sizeof(char));
	hmac = calloc(bit_size+1, sizeof(char));
	if(byte_secret == 0
			|| byte_string == 0
			|| hmac == 0
			|| otp_int_to_bytestring(input, byte_string) == 0
			|| otp_byte_secret(data, secret_len, byte_secret) == 0
			|| (*(data->algo))(byte_secret, byte_string, hmac) == 0)
		goto exit;
	
	// gather hmac's offset, piece together code
	int offset = (hmac[bit_size-1] & 0xF);
	code =
		((hmac[offset] & 0x7F) << 24 |
		(hmac[offset+1] & 0xFF) << 16 |
		(hmac[offset+2] & 0xFF) << 8 |
		(hmac[offset+3] & 0xFF));
	code %= (int)pow(10, data->digits);
	
	// write out the char array code, if requested
	if(out_str != NULL)
		sprintf(out_str, (char[]){'%', '0', data->digits + 48, 'd', '\0'}, code);
	
exit:
	free(hmac);
	free(byte_string);
	free(byte_secret);
	return code;
}


