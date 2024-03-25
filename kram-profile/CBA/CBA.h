#import "Foundation/Foundation.h"

// TODO: move to header
@interface CBA : NSObject

- (_Nonnull instancetype)init;
- (void)deinit;

- (void)parse:(NSData* _Nonnull)file filename:(NSString* _Nonnull)filename;
- (void)parseAll:(NSArray<NSData*> * _Nonnull)files filenames:(NSArray<NSString*> * _Nonnull)filenames;

- (NSString* _Nonnull)analyze;

@end
