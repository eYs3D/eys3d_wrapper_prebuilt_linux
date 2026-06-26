#pragma once
#include <vector>
#include <map>
#include <string>

struct sqlite3;
class ModeConfig
{
public:

    enum IMU_TYPE
    {
        IMU_NONE,
        IMU_6Axis = 6,
        IMU_9Axis = 9,
    };
    struct MODE_CONFIG
    {
        enum DECODE_TYPE
        {
            YUYV,
            MJPEG
        };
        struct RESOLUTION
        {
            int Width;
            int Height;

            RESOLUTION() : Width( 0 ), Height( 0 ) {}
        };       
        int                iMode;
        int                iUSB_Type;
        int                iInterLeaveModeFPS;
        bool               bRectifyMode;
        DECODE_TYPE        eDecodeType_L;
        DECODE_TYPE        eDecodeType_K;
        DECODE_TYPE        eDecodeType_T;
        RESOLUTION         L_Resolution;
        RESOLUTION         D_Resolution;
        RESOLUTION         K_Resolution;
        RESOLUTION         T_Resolution;      
        std::vector< int > vecDepthType;   
        std::vector< int > vecColorFps;
        std::vector< int > vecDepthFps;
        //QString            csModeDesc;
	std::string	   csModeDesc;
        unsigned short videoModeD11OrColorOnly = 0xff; // v2 table added
        unsigned short videoModeZ14 = 0xff;            // v2 table added
        int rectifyFileIndex = 0xff;                   // v2 table added
        // used by grape (eSP936 / 80363) - read straight from the table
        int                iL_DataType;
        int                iK_DataType;
        int                iT_DataType;
        int                iL_VideoMode;
        int                iK_VideoMode;
        int                iT_VideoMode;
        int                iL_FPS;
        int                iK_FPS;
        int                iT_FPS;
        int                iL_Index;
        int                iK_Index;
        std::vector< int > iT_Index;
        MODE_CONFIG() : eDecodeType_L( YUYV ), eDecodeType_K( YUYV ),  eDecodeType_T( YUYV ), iInterLeaveModeFPS( 0 ),
                        iL_DataType( 0 ), iK_DataType( 0 ), iT_DataType( 0 ),
                        iL_VideoMode( 0 ), iK_VideoMode( 0 ), iT_VideoMode( 0 ),
                        iL_FPS( 0 ), iK_FPS( 0 ), iT_FPS( 0 ), iL_Index( 0 ), iK_Index( 0 ) {}
    };
    const std::vector< MODE_CONFIG >& GetModeConfigList( const int iPID );
    IMU_TYPE GetIMU_Type( const int iPID );

    ModeConfig();
    ~ModeConfig();

private:

    struct PID_TABLE
    {
        //QString csTableName;
	std::string csTableName;
        IMU_TYPE IMU_Type;

        std::vector< MODE_CONFIG > vecModeConfig;

        PID_TABLE() : IMU_Type( IMU_NONE ) {}
    };
    //ModeConfig();
    //~ModeConfig();

    sqlite3* m_sq3;
    std::map< int, PID_TABLE > m_mapDeviceTable;
    bool IsSupportedColumnNumber(int counts);
    void ReadModeConfig(const char *path);
};

