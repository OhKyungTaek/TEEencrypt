/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include <TEEencrypt_ta.h>
#define MAX_LEN 100
#define RSA_KEY_SIZE 1024
#define RSA_MAX_PLAIN_LEN_1024 86 // (1024/8) - 42 (padding)
#define RSA_CIPHER_LEN_1024 (RSA_KEY_SIZE / 8)

void rsa_gen_keys(int *sess) { //struct ta_attrs *ta) {
	TEEC_Result res;

	res = TEEC_InvokeCommand(sess, TA_RSA_CMD_GENKEYS, NULL, NULL);
	if (res != TEEC_SUCCESS)
		errx(1, "\nTEEC_InvokeCommand(TA_RSA_CMD_GENKEYS) failed %#x\n", res);
	printf("====Keys generated====\n");
}

int main(int argc, char *argv[])
{
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_TEEencrypt_UUID;
	uint32_t err_origin;

	char enc_cmd[] = "-e";
	char dec_cmd[] = "-d";
	char rsa_cmd[] = "RSA";
	char csr_cmd[] = "Caesar";
	char plaintext[MAX_LEN]={0,};
	char ciphertext[MAX_LEN]={0,};
	char keytext[MAX_LEN]={0,};
	char clear[RSA_MAX_PLAIN_LEN_1024];
	char ciph[RSA_CIPHER_LEN_1024];
	

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	res = TEEC_OpenSession(&ctx, &sess, &uuid,
			       TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			res, err_origin);
	
	memset(&op, 0, sizeof(op));
					// TEEC_VALUE_INOUT
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_OUTPUT,	// Caesar Enc-Dec
					 TEEC_VALUE_INOUT,		// Caesar RandomKey
					 TEEC_MEMREF_TEMP_INPUT, 	// RSA	Enc-Dec
					 TEEC_MEMREF_TEMP_OUTPUT);	// RSA	Enc-Dec
	op.params[0].tmpref.buffer = plaintext;
	op.params[0].tmpref.size = MAX_LEN;

	if(argc == 0 || argc > 5){
		printf("Invalid execution statement.\n");
		TEEC_CloseSession(&sess);
		TEEC_FinalizeContext(&ctx);
		return 1;
	}

	// argv 0:TEEencrypt 1:-e, 2:text file
	if(!strcmp(enc_cmd, argv[1])){
	// Encryption
		TEEC_Result ret;
		// Read file
		FILE* fs;
		fs = fopen(argv[2], "r");
		fread(plaintext, sizeof(plaintext), 1, fs);
		fclose(fs);
		printf("======PlainText=======\n%s\n",plaintext);
		memcpy(op.params[0].tmpref.buffer, plaintext, MAX_LEN);
		char *cipher = NULL;

		// InvokeCommand
		if(!strcmp(argv[3], csr_cmd)){	// Caesar encrypt
			res = TEEC_InvokeCommand(&sess, TA_TEEencrypt_CMD_INC_VALUE, &op, 						&err_origin);
			if (ret != TEEC_SUCCESS)
				errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x", ret, err_origin);
			memcpy(ciphertext, op.params[0].tmpref.buffer, MAX_LEN);
			int key_value = op.params[1].value.a;
			char buf[10]={0,};
			sprintf(buf, "%d", key_value);
			FILE *fk = fopen("ciphertext_key.txt", "w");
			fwrite(buf, strlen(buf), 1, fk); 
			fclose(fk);
			cipher = ciphertext;
		}
		else if(!strcmp(argv[3], rsa_cmd)){	// RSA encrypt
			op.params[2].tmpref.buffer = clear;
			op.params[2].tmpref.size = RSA_MAX_PLAIN_LEN_1024;
			op.params[3].tmpref.buffer = ciph;
			op.params[3].tmpref.size = RSA_CIPHER_LEN_1024;

			rsa_gen_keys(&sess);
			ret = TEEC_InvokeCommand(&sess, TA_RSA_CMD_ENCRYPT, &op, &err_origin);
			if (ret != TEEC_SUCCESS)
				errx(1, "\nTEEC_InvokeCommand(TA_RSA_CMD_ENCRYPT) failed 0x%x origin 0x%x\n", ret, err_origin);
			//printf("\nThe text sent was encrypted: %s\n", ciph);
			cipher = ciph;
		}else{
			printf("Invalid execution statement.\n");
			TEEC_CloseSession(&sess);
			TEEC_FinalizeContext(&ctx);		
			return 1;
		}

		if(ret == TEEC_SUCCESS){
			printf("======Ciphertext======\n%s", cipher);
			// Write file
			
			FILE *fc = fopen("ciphertext.txt", "w");
			fwrite(cipher, strlen(cipher), 1, fc); 
			fclose(fc);

			printf("*Successful Saving\n");
		}
	}
	else if(!strcmp(dec_cmd, argv[1])){
	// Decryption 
		TEEC_Result ret;
		// Read CipherText
		FILE* fc = fopen(argv[2], "r");
		fread(ciphertext, sizeof(ciphertext), 1, fc);
		fclose(fc);
		printf("======Ciphertext======\n%s\n", ciphertext);
		char *plain = NULL;

		// InvokeCommand
		if(!strcmp(argv[4], csr_cmd)){	// Caesar decrypt
			// Read KeyValue
			FILE* fk = fopen(argv[3], "r");
			fread(keytext, sizeof(keytext), 1, fk);
			fclose(fk);
			int value = 0;
			value = atoi(keytext);
			printf("======Key Value=======\n%d\n\n",value);
			op.params[1].value.a = value;

			memcpy(op.params[0].tmpref.buffer, ciphertext, MAX_LEN);
			ret = TEEC_InvokeCommand(&sess, TA_TEEencrypt_CMD_DEC_VALUE, &op, 							&err_origin);
			if (ret != TEEC_SUCCESS)
				errx(1, "TEEC_InvokeCommand failed with code 0x%x origin 0x%x",
					ret, err_origin);
			memcpy(plaintext, op.params[0].tmpref.buffer, MAX_LEN);
			plain = plaintext;
		}else{
			printf("Invalid execution statement.\n");
			TEEC_CloseSession(&sess);
			TEEC_FinalizeContext(&ctx);
			return 1;
		}
		if(ret == TEEC_SUCCESS){
			printf("======Plaintext=======\n%s", plain);

			// Write file
			FILE *fp = fopen("plaintext_dec.txt", "w");
			fwrite(plain, strlen(plain), 1, fp); 
			fclose(fp);
		}
	}
	

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return 0;
}
