#import "Foundation/Foundation.h"

// TODO: move to header
@interface CBA : NSObject

- (_Nonnull instancetype)init;
- (void)deinit;

// Can parseAll or one file at time
- (void)parse:(NSData* _Nonnull)file filename:(NSString* _Nonnull)filename;
- (void)parseAll:(NSArray<NSData*> * _Nonnull)files filenames:(NSArray<NSString*> * _Nonnull)filenames;

// This isn't so useful, since need specific files to parse
- (NSString* _Nonnull)analyzeAll;
- (NSString* _Nonnull)analyze:(NSArray<NSString*> * _Nonnull)filenames;

@end
