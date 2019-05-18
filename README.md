# Matrix stickers

## Rationale

Поскольку разработчики Matrix/Riot.im не особо спешат реализовывать возможность использования юзерских стикеров, приходится дорабатывать эти возможности со своей стороны. Программа позволяет создавать стикерпаки и отправлять в комнаты Matrix свои собственные стикеры.

## Features

1. Загрузка стикеров через программу на ваш домашний сервер Matrix и реиспользование их при отправке
1. Экспорт и импорт стикерпаков
1. Простановка тегов для стикеров
1. Поиск по описанию и тегам (как внутри одного стикерпака, так и по всем имеющимся)
1. Клавиатурная навигация для большинства основных функций
1. Мини-интерфейс для использования в качестве сайдбара (убираются кнопки и подписи), активируется при сужении окна по горизонтали

## Установка
Необходимо наличие установленного Qt >=5.7
1. Склонируйте репозиторий и перейдите в каталог
```
git clone https://github.com/rkfg/mxstickers.git
cd mxstickers
```
2. Сгенерируйте Makefile и запустите сборку
```
qmake
make
```
3. Запустите полученный бинарный файл
```
./mxstickers
```
