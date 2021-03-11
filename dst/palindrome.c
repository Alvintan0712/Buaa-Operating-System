#include <stdio.h>
#include <string.h>
int main()
{
	int n;
	scanf("%d",&n);

	char s[10001] = {0};
	sprintf(s, "%d", n);
	int  i, len = strlen(s), flag = 1;
	for (i = 0; i < len >> 1; i++) {
		if (s[i] != s[len - i - 1])
			flag = 0;
	}
	
	if(flag){
		printf("Y\n");
	}else{
		printf("N\n");
	}
	return 0;
}
