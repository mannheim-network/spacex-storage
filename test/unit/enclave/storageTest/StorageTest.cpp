#include "StorageTest.h"

bool test_get_hashs_from_block()
{
    std::string block1_hex_str = "122a0a22122035d209a850b6909fa3dad99c3f96415f1a4b92ac23062c92cd3562b6bef3b7c11200188e8010122a0a2212201825c65359a2f165ea0c0301a6dac34176e3742c96cd12449006b086205ac3781200188e8010122a0a2212201687310fe656495e943b4b57c709a487f94d7e00e310b0303781c04ee890e3221200188e8010122a0a2212206e11ef1a3bb54778d417c3f91bc2b557448b2567a3d0f3598a33316fd1a36c271200188e8010122a0a2212208ad1b49139d0c987ed74c5df798c039d3c6eb034907284778974bd63abadc658120018ee91030a1a080218e091432080801020808010208080102080801020e09103";

    uint8_t *block1_data = hex_string_to_bytes(block1_hex_str.c_str(), block1_hex_str.size());
    std::vector<uint8_t *> hashs;
    spacex_status_t spacex_status = get_hashs_from_block(block1_data, block1_hex_str.size() / 2, hashs);

    if (block1_data != NULL)
    {
        delete block1_data;
    }

    // Assert
    if (spacex_status != SPACEX_SUCCESS)
    {
        log_err("Get hashs from block error, code is %d\n", spacex_status);
        return false;
    }

    if (hashs.size() != 5)
    {
        log_err("Get hashs failed, block must have 5 hashs\n");
        return false;
    }

    std::string hash_str = hexstring_safe(hashs[2], HASH_LENGTH);
    if (hash_str != "1687310fe656495e943b4b57c709a487f94d7e00e310b0303781c04ee890e322")
    {
        log_err("The 3rd hash must be 1687310fe656495e943b4b57c709a487f94d7e00e310b0303781c04ee890e322\n");
        return false;
    }
    
    // Clear hashs
    for (size_t i = 0; i < hashs.size(); i++)
    {
        delete hashs[i];
    }
    hashs.clear();

    return true;
}