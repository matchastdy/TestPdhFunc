#include <iostream>

#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <pdh.h>

#include <string>
#include <tchar.h>

#include <pdhmsg.h>

int test() {
	PDH_HQUERY              hQuery;
	PDH_HCOUNTER            hCounter;
	PDH_FMT_COUNTERVALUE    fmtValue;
	PDH_STATUS				status;
	
	/*1 クエリを作成する*/
	status = PdhOpenQuery(NULL, 0, &hQuery);
	if (status != ERROR_SUCCESS) {
		return 1;
	}

	/*2 クエリにカウンターを追加する
	* この例ではパフォーマンスカウンターでの「\Processor(_Total)\% Processor Time」を追加している
	*/
	status = PdhAddCounter(hQuery, TEXT("\\Processor(_Total)\\% Processor Time"), 0, &hCounter);
	if (status != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/*3 パフォーマンスデータを収集する
	* 3-1 多くのカウンターではデータ値を計算するために2つのサンプルデータが必要とのこと
	* そのため、最初に一度サンプルデータの収集を行う
	*/
	status = PdhCollectQueryData(hQuery);
	PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &fmtValue);
	if (status != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	// キーが押されるまでループ
	while (!_kbhit()) {
		/*3-2 コレクション間で少なくとも１秒以上待機する*/
		Sleep(1000);
		system("CLS");

		/*3-3 再度サンプルデータを収集する*/
		PdhCollectQueryData(hQuery);

		/*4 サンプル取得後、表示可能な値を計算する
		* この例ではDOUBLEがたに変換して表示
		*/
		PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, NULL, &fmtValue);
		printf("doubleValue = %.15f\n", fmtValue.doubleValue);
	}

	/*5 クエリのエータ収集が完了したら、けりを閉じてシステムリソースを開放する
	* クエリに関連付けられているすべてのカウンターハンドルを閉じる
	*/
	PdhCloseQuery(hQuery);
	return 0;
}

int test2() {
	PDH_HQUERY              hQuery;
	HCOUNTER				hCounter[512];
	PDH_FMT_COUNTERVALUE	fmtValue[512];
	PDH_STATUS				status;

	status = PdhOpenQuery(NULL, 0, &hQuery);
	if (status != ERROR_SUCCESS) {
		return 1;
	}

	/* ワイルドカードを含むカウンターパス
	*  今回は\Process(*)\% Processor Timeとする。
	*/
	LPCWSTR					wildCardPath = TEXT("\\Process(*)\\% Processor Time"); // 
	unsigned long			pathListLength = 0; // カウンターパスリストのサイズ数
	/* PdhExpandWildCardPathは2回実行しなければならない。
	*  1回目は取得するカウンターパスリストのサイズを取得する。
	*  ちなみに、カウンターパスリストを格納する変数はない（NULL）ので、
	*  正常に動作していれば、statusはPDH_MORE_DATAが返ってくるはず。
	*/
	status = PdhExpandWildCardPath(NULL, wildCardPath, NULL, &pathListLength, 0);
	if (status != ERROR_SUCCESS && status != PDH_MORE_DATA) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/* カウンターパスリストのサイズがunsigned longの最大値となっていないかを確認
	*  最大値の場合、次のメモリ確保の時にバッファオーバフローが発生するため、処理を終了する。
	*  （おそらく、そんなことはないとは思うが）
	*/
	if (pathListLength == ULONG_MAX) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	/* カウンターパスリストのサイズ + 1の分だけ、メモリを確保する。
	*  カウンターパスは最後はNULL文字で終わっていないといけないので、その分が増えている。
	*/
	TCHAR*					expandedPathList = (TCHAR*)calloc(pathListLength + 1, sizeof(TCHAR));
	/* PdhExpandWildCardPathを再度実行し、*/
	PdhExpandWildCardPath(NULL, wildCardPath, expandedPathList, &pathListLength, 0);


	TCHAR*					epl = expandedPathList;
	int						pathListNum = 0;

	if (epl == NULL) {
		return 0;
	}

	while (*epl) {
		PdhAddCounter(hQuery, epl, 0, &hCounter[pathListNum]);
		pathListNum++;
		printf("%zd\n", wcslen(epl));
		epl += wcslen(epl);
		epl += 1;
		//epl += wcslen(epl) + 1;
	}

	status = PdhCollectQueryData(hQuery);
	if (status != ERROR_SUCCESS) {
		PdhCloseQuery(hQuery);
		return 1;
	}

	while (!_kbhit()) {
		Sleep(10000);
		PdhCollectQueryData(hQuery);
		for (int i = 0; i < pathListNum; i++) {
			unsigned long		pdwBufferSize = 0;

			PdhGetCounterInfo(hCounter[i], TRUE, &pdwBufferSize, NULL);

			PPDH_COUNTER_INFO lpBuffer = (PPDH_COUNTER_INFO)calloc(pdwBufferSize, sizeof(PPDH_COUNTER_INFO));
			PdhGetCounterInfo(hCounter[i], TRUE, &pdwBufferSize, lpBuffer);

			if (lpBuffer == nullptr) {
				continue;
			}

			printf("%ls ", lpBuffer->szFullPath);

			PdhGetFormattedCounterValue(hCounter[i], PDH_FMT_DOUBLE, NULL, &fmtValue[i]);

			printf("doubleValue = %.15f\n", fmtValue[i].doubleValue);

			free(lpBuffer);
			lpBuffer = NULL;
		}
	}

	/*5 クエリのエータ収集が完了したら、けりを閉じてシステムリソースを開放する
	* クエリに関連付けられているすべてのカウンターハンドルを閉じる
	*/
	PdhCloseQuery(hQuery);
	return 0;
}

int main()
{
	//test();
	test2();

	//PDH_HQUERY              hQuery;
	//HCOUNTER				hCounter[512];
	//PDH_FMT_COUNTERVALUE	fmtValue[512];

	//TCHAR* cp;

	//int     numProcessor = 0;

	//HRESULT hr;

	//hr = PdhOpenQuery(NULL, 0, &hQuery);

	//if (hr != ERROR_SUCCESS) {
	//	return 0;
	//}


	//LPCWSTR  wildCardPath = TEXT("\\Process(*)\\% Processor Time");
	//unsigned long pathListLength = 0;
	//PdhExpandWildCardPath(NULL, wildCardPath, NULL, &pathListLength, 0);

	//TCHAR* mszExpandedPathList = (TCHAR*)calloc(pathListLength + 1, sizeof(TCHAR));

	//PdhExpandWildCardPath(NULL, wildCardPath, mszExpandedPathList, &pathListLength, 0);

	//cp = mszExpandedPathList;

	//if (cp == NULL) {
	//	return 0;
	//}

	//while (*cp) {
	//	PdhAddCounter(hQuery, cp, 0, &hCounter[numProcessor]);
	//	numProcessor++;
	//	cp += wcslen(cp) + 1;
	//}

	//PdhCollectQueryData(hQuery);

	//while (!_kbhit()) {
	//	Sleep(10000);
	//	//system("CLS");
	//	PdhCollectQueryData(hQuery);
	//	for (int i = 0; i < numProcessor; i++) {
	//		unsigned long		pdwBufferSize = 0;

	//		PdhGetCounterInfo(hCounter[i], TRUE, &pdwBufferSize, NULL);

	//		PPDH_COUNTER_INFO lpBuffer = (PPDH_COUNTER_INFO)calloc(pdwBufferSize, sizeof(PPDH_COUNTER_INFO));
	//		PdhGetCounterInfo(hCounter[i], TRUE, &pdwBufferSize, lpBuffer);

	//		if (lpBuffer == nullptr) {
	//			continue;
	//		}

	//		printf("%ls ", lpBuffer->szFullPath);

	//		PdhGetFormattedCounterValue(hCounter[i], PDH_FMT_DOUBLE, NULL, &fmtValue[i]);

	//		printf("doubleValue = %.15f\n", fmtValue[i].doubleValue);

	//		free(lpBuffer);
	//		lpBuffer = NULL;
	//	}
	//}
	//_getch();
	//PdhCloseQuery(hQuery);
	//return 0;
}