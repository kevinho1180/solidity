object "main" {
    code {
        let v0 := calldataload(0)
        let v1 := calldataload(1)
        let v2 := calldataload(2)
        let v3 := calldataload(3)
        let v4 := calldataload(4)
        let v5 := calldataload(5)
        let v6 := calldataload(6)
        let v7 := calldataload(7)
        let v8 := calldataload(8)
        let v9 := calldataload(9)
        let v10 := calldataload(10)
        let v11 := calldataload(11)
        let v12 := calldataload(12)
        let v13 := calldataload(13)
        let v14 := calldataload(14)
        let v15 := calldataload(15)
        sstore(16, v0)
        sstore(15, v15)
        sstore(14, v14)
        sstore(13, v13)
        sstore(12, v12)
        sstore(11, v11)
        sstore(10, v10)
        sstore(9, v9)
        sstore(8, v8)
        sstore(7, v7)
        sstore(6, v6)
        sstore(5, v5)
        sstore(4, v4)
        sstore(3, v3)
        sstore(2, v2)
        sstore(1, v1)
        sstore(0, v0)
    }
}
// ====
// stackOptimization: true
// ----
//     /* "":58:59   */
//   0x00
//     /* "":45:60   */
//   calldataload
//     /* "":92:93   */
//   0x01
//     /* "":79:94   */
//   calldataload
//     /* "":126:127   */
//   0x02
//     /* "":113:128   */
//   calldataload
//     /* "":160:161   */
//   0x03
//     /* "":147:162   */
//   calldataload
//     /* "":194:195   */
//   0x04
//     /* "":181:196   */
//   calldataload
//     /* "":228:229   */
//   0x05
//     /* "":215:230   */
//   calldataload
//     /* "":262:263   */
//   0x06
//     /* "":249:264   */
//   calldataload
//     /* "":296:297   */
//   0x07
//     /* "":283:298   */
//   calldataload
//     /* "":330:331   */
//   0x08
//     /* "":317:332   */
//   calldataload
//     /* "":364:365   */
//   0x09
//     /* "":351:366   */
//   calldataload
//     /* "":399:401   */
//   0x0a
//     /* "":386:402   */
//   calldataload
//     /* "":435:437   */
//   0x0b
//     /* "":422:438   */
//   calldataload
//     /* "":471:473   */
//   0x0c
//     /* "":458:474   */
//   calldataload
//     /* "":507:509   */
//   0x0d
//     /* "":494:510   */
//   calldataload
//     /* "":543:545   */
//   0x0e
//     /* "":530:546   */
//   calldataload
//     /* "":579:581   */
//   0x0f
//     /* "":566:582   */
//   calldataload
//     /* "":591:605   */
//   dup16
//     /* "":598:600   */
//   0x10
//     /* "":591:605   */
//   sstore
//     /* "":621:623   */
//   0x0f
//     /* "":614:629   */
//   sstore
//     /* "":645:647   */
//   0x0e
//     /* "":638:653   */
//   sstore
//     /* "":669:671   */
//   0x0d
//     /* "":662:677   */
//   sstore
//     /* "":693:695   */
//   0x0c
//     /* "":686:701   */
//   sstore
//     /* "":717:719   */
//   0x0b
//     /* "":710:725   */
//   sstore
//     /* "":741:743   */
//   0x0a
//     /* "":734:749   */
//   sstore
//     /* "":765:766   */
//   0x09
//     /* "":758:771   */
//   sstore
//     /* "":787:788   */
//   0x08
//     /* "":780:793   */
//   sstore
//     /* "":809:810   */
//   0x07
//     /* "":802:815   */
//   sstore
//     /* "":831:832   */
//   0x06
//     /* "":824:837   */
//   sstore
//     /* "":853:854   */
//   0x05
//     /* "":846:859   */
//   sstore
//     /* "":875:876   */
//   0x04
//     /* "":868:881   */
//   sstore
//     /* "":897:898   */
//   0x03
//     /* "":890:903   */
//   sstore
//     /* "":919:920   */
//   0x02
//     /* "":912:925   */
//   sstore
//     /* "":941:942   */
//   0x01
//     /* "":934:947   */
//   sstore
//     /* "":963:964   */
//   0x00
//     /* "":956:969   */
//   sstore
//     /* "":25:975   */
//   stop
