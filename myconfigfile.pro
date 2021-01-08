-injars       ../evio/build/lib/jevio-6.0.jar
-injars       ../evio/java/jars/disruptor-3.4.jar
-injars       ../evio/java/jars/lz4-java-1.4.0.jar
-outjars      ../evio/build/lib/jevio-6.0.optimized.jar
-libraryjars  <java.home>/jre/lib/rt.jar

-optimizationpasses 3
-overloadaggressively
-verbose

-keepparameternames
-renamesourcefileattribute SourceFile
-keepattributes Exceptions,InnerClasses,Signature,Deprecated,
                SourceFile,LineNumberTable,*Annotation*,EnclosingMethod

-keep public class * {
    public protected *;
}

-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

-keepclassmembers,allowoptimization enum * {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    private static final java.io.ObjectStreamField[] serialPersistentFields;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}
