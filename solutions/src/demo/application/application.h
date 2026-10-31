/**
 * @file application.h
 * @brief デモアプリケーションの実行 API を定義する。
 * @date 2026-10-31
 * @par 最終更新日 2026-10-31
 */
#pragma once
#ifndef DYNAMIC_WIND_DEMO_DEMO_APPLICATION_APPLICATION_H
#define DYNAMIC_WIND_DEMO_DEMO_APPLICATION_APPLICATION_H

namespace demo::application
{

/**
 * @brief ウィンドウと描画環境を初期化してアプリケーションを実行する。
 * @return アプリケーションの終了コード。
 */
int Run();

}
#endif
